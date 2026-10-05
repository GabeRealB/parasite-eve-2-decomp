#ifndef GAMEPLAY_MODEL_LIGHTING_H
#define GAMEPLAY_MODEL_LIGHTING_H

#include "types.h"

#include "main/tmd_types.h"

void Gp_ApplyPadReplay(s32 arg0, u16* arg1);

/// Sets the shared material greys used by environment-layer model drawing.
///
/// `layerIntensity` is clamped to 0..255 and copied to the layer's three RGB
/// bytes. Nonpositive input sets the base RGB to neutral 128; positive input
/// sets it to floor((255 - clamped intensity) / 2). Material code bytes are
/// preserved. These are material inputs to lighting, not final packet colours.
/// The state is shared by all environment-layer draws and persists until the
/// next call; callers update it for the cloak/translucency state being drawn.
void modelLightingSetLayerMaterials(s32 layerIntensity);

/// Projects and lights layered corners, deriving the environment layer's texture coordinates.
///
/// Draw resolution selects this pre-pass for `0x40C8` except stage 2 area 16.
/// Each element has two words: u16 vertex/normal byte references, then two
/// u16 byte offsets from `preXformWrite` to layer/base corner colour groups.
/// Geometry offsets are masked with 0xFFF8 and must select complete SVECTORs.
/// The vertex reference shifted right by three must fit the 1024-entry depth
/// cache. Consecutive identical references reuse screen XY and cached screen Z;
/// projection failures retain Z and set `TMD_VERTEX_DEPTH_INVALID`.
///
/// Each destination requires aligned four-byte colour and XY stores; the layer
/// also requires two UV bytes at colour-group bytes 8..9. Destination extents
/// must cover layer offset + 10 and base offset + 8 inside the first region.
/// Both corners receive the same projected pixel coordinates and are lit from
/// the shared layer/base materials. Below 4096, the layer RGB is interpolated
/// with neutral grey using the object's 12-fraction-bit `colorBlend`.
///
/// Environment U/V subtract the rotated normal scaled by GPF12 with
/// `colorBlend >> 9` from screen XY + (160,120). Negative values clamp to zero;
/// U >= 256 rebases by 128 and clamps to 191; V clamps to 239. The layer's
/// colour-group byte 3 stores a temporary 0/1 second-page marker, consumed by
/// the pre-transformed environment drawers before they stamp the command.
///
/// The caller supplies the GTE projection/light matrices and borrowed geometry,
/// depth cache and writable packet region; capacities are unchecked. With initial
/// count 0..65535 and stride in u32 words (at least two), returns the cursor after
/// count * stride words. Neither packet cursor advances. A zero count stays zero;
/// a positive count is consumed to -1. `objectFlags` is ignored. No storage is retained.
u32* tmdXformStreamVertsEnvLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects and lights paired corners for an offset-texture layer and its base.
///
/// Draw resolution selects this pre-pass for `0x40C8` in stage 2 area 16.
/// Each element has two words: vertex/normal u16 byte references, then layer
/// and base u16 byte offsets from `preXformWrite` to corner colour groups.
/// Masked geometry references must select complete eight-byte SVECTORs in
/// the borrowed arrays. The vertex reference >> 3 must fit the 1024-entry
/// depth cache; consecutive identical references reuse screen XY and cached Z.
/// `TMD_GTE_ERROR_FLAG` marks fresh screen Z with `TMD_VERTEX_DEPTH_INVALID`.
///
/// Both destinations receive a four-byte RGB/code word and four-byte pixel XY
/// immediately after it; each must be aligned and provide eight writable bytes
/// in the first packet region. Lighting uses complementary material greys:
/// layer = colorBlend >> 5, base = 128 - layer, where the object's expected
/// `colorBlend` range is 0..4096 (twelve fractional bits). There is no clamp;
/// RGB stores truncate to bytes. Material code bytes come from the neutral
/// constants. Texture fields are preserved. The workspace also saves screen XY
/// in `texCoord` and the rotated normal in `elemNormal`; no UVs are calculated.
///
/// The caller supplies GTE projection/lighting, borrowed geometry and depth
/// cache, and the writable packet region. Capacities are unchecked. `elements`
/// starts after the three-word header; count is 0..65535 and stride is in u32
/// words, at least two for a nonempty record. Returns the cursor after count *
/// stride words without advancing either packet cursor. Zero count stays zero;
/// positive count ends at -1. `objectFlags` is ignored; no storage is retained.
u32* tmdXformStreamVertsOffsetLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects and lights environment/base Gouraud triangle pairs from layered TMD records.
///
/// Draw resolution selects this callback for `0x4038` except stage 2 area 16.
/// Words 0..2 pack three vertex and three normal byte references. Construction
/// requires at least six words and initializes only the opaque base's texture;
/// this callback supplies the environment layer's UV/page and both packets'
/// positions, lighting, length and command. The layer and base use the shared
/// material colours; layer RGB also interpolates with neutral grey below the
/// object's full 4096 blend weight. The object's blend has twelve fractional bits.
///
/// Projection under `TMD_GTE_ERROR_FLAG` or NCLIP(0,1,2) <= 0 rejects both
/// packets. Environment UVs use screen pixels + (160,120) minus rotated normals
/// scaled by GPF12 with `colorBlend >> 9`. U >= 256 rebases by 128 and clamps
/// to 191; negative U/V clamp to zero and V clamps to 239. A temporary marker
/// in each colour group's high byte records rebasing. Any marked corner selects
/// the additive direct-colour page at VRAM (576,256); other corners then subtract
/// 128 from U when U >= 128, otherwise clamp to zero. With no markers the page
/// is (448,256). Command 0x36 replaces the first marker; command 0x34 completes
/// the opaque base. The layer is linked first and the base second at AVSZ3 depth.
///
/// `elements` begins after the three-word header. Initial `elemCount` is
/// 0..65535; `elemStride` counts u32 words. Full strides must be readable and
/// masked geometry references must select complete eight-byte SVECTORs in their
/// respective borrowed arrays; low-bit roles and array extents are unproven.
/// The caller supplies GTE transform/lighting and the relevant depth scale.
/// `primWrite` must provide two aligned 40-byte `POLY_GT3` slot(s) per element
/// in the selected half's second region. Packet capacities are unchecked.
///
/// Accepted packets prepend to the displaced OT at
/// `(((u32)OTZ << gDisplayState.otDepthShift) >> 4) & 1023`; normal shifts are
/// 0..3. Every bucket must fit its backing table. DMA links encode 24 address
/// bits; packet and OT storage must remain GPU-visible until consumption ends.
/// Advances `primWrite` by two slot(s) per element, including rejected ones,
/// and returns `elements + initial count * elemStride`, leaving the next marker
/// unconsumed. The count ends at -1 even for an empty record, which reads no
/// payload and advances neither cursor. `preXformWrite` is unchanged.
/// `objectFlags` is ignored, including reverse-culling flags. Storage is borrowed;
/// no pointer is retained.
u32* tmdDrawStreamPrimGt3EnvLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects and lights offset-textured layer/base Gouraud triangle pairs.
///
/// Draw resolution selects this callback for `0x4038` in stage 2 area 16.
/// Words 0..2 pack three vertex and three normal byte references; construction
/// requires at least six words. Both textures are supplied by
/// `tmdBuildStreamGt3OffsetLayer` during construction, whose location gate also
/// includes area 15. This callback preserves UV/CLUT and the page location.
///
/// The object's twelve-fraction-bit `colorBlend`, shifted right five, supplies
/// the layer's 0..128 grey intensity; the base uses its complement to 128.
/// Both packets are lit from the three normals. `TMD_GTE_ERROR_FLAG` or
/// NCLIP(0,1,2) <= 0 rejects the pair. Accepted packets receive commands
/// 0x36/0x34 and the layer page's ABR bit 5 is set, preserving bit 6 (mode 1
/// or 3). The layer is prepended first, then the base, at the same AVSZ3 depth.
/// DMA links retain 24 address bits and preserve both tags' high length byte.
///
/// `elements` begins after the three-word header. Initial `elemCount` is
/// 0..65535; `elemStride` counts u32 words. Full strides must be readable and
/// masked geometry references must select complete eight-byte SVECTORs in their
/// respective borrowed arrays; low-bit roles and array extents are unproven.
/// The caller supplies GTE transform/lighting and the relevant depth scale.
/// `primWrite` must provide two aligned 40-byte `POLY_GT3` slot(s) per element
/// in the selected half's second region. Packet capacities are unchecked.
///
/// Accepted packets prepend to the displaced OT at
/// `(((u32)OTZ << gDisplayState.otDepthShift) >> 4) & 1023`; normal shifts are
/// 0..3. Every bucket must fit its backing table. DMA links encode 24 address
/// bits; packet and OT storage must remain GPU-visible until consumption ends.
/// Advances `primWrite` by two slot(s) per element, including rejected ones,
/// and returns `elements + initial count * elemStride`, leaving the next marker
/// unconsumed. The count ends at -1 even for an empty record, which reads no
/// payload and advances neither cursor. `preXformWrite` is unchanged.
/// `objectFlags` is ignored, including reverse-culling flags. Storage is borrowed;
/// no pointer is retained.
u32* tmdDrawStreamPrimGt3OffsetLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects and lights an offset-texture layer/base pair of Gouraud textured quads.
///
/// Draw resolution selects this callback for `0x4078` in stage 2 area 16.
/// Words 0..3 pack four vertex u16 byte references followed by four normal
/// references. Construction requires at least seven words and seeds both
/// packets' texture fields with independent layer/base page and CLUT offsets.
/// Masked geometry references must select complete eight-byte SVECTORs in the
/// borrowed arrays; their full extents and the low reference bits are unproven.
///
/// `TMD_GTE_ERROR_FLAG` rejects either projection. After both succeed, the quad
/// is accepted if NCLIP(0,1,2) > 0, or otherwise NCLIP(1,2,3) < 0. Earlier XY
/// stores can remain in rejected packets. Both packets are lit from the same
/// corner normals under complementary greys: layer = colorBlend >> 5, base =
/// 128 - layer, for expected `colorBlend` 0..4096. RGB stores truncate without
/// clamping. Accepted packets receive twelve payload words, layer command 0x3E
/// and opaque base command 0x3C. Texture fields persist. `objectFlags` and the
/// object's semi-transparency/reverse-culling flags do not select variants here.
///
/// `elements` starts after the three-word header; count is 0..65535 and stride
/// counts u32 words. The caller supplies GTE projection/lighting and ZSF4, and
/// `primWrite` provides two aligned, writable 52-byte POLY_GT4 slots per element
/// in the second region. Each full element stride must be readable. Capacities
/// are unchecked. AVSZ4 selects `(((u32)OTZ << gDisplayState.otDepthShift) >> 4)
/// & 1023` in the displaced OT; normal shifts are 0..3 and buckets must fit it.
/// Layer then base are prepended, so GPU traversal draws base before layer.
/// DMA links retain 24 address bits; packets and OT stay GPU-visible until used.
/// Advances `primWrite` by two slots even on rejection and returns the cursor
/// after count * stride words, leaving the next marker. Count ends at -1,
/// including on empty input. `preXformWrite` is unchanged. No storage is retained.
u32* tmdDrawStreamPrimGt4OffsetLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects and lights environment/base Gouraud quad pairs from layered TMD records.
///
/// Draw resolution selects this callback for `0x4078` except stage 2 area 16.
/// Words 0..3 pack four vertex and four normal byte references. Construction
/// requires at least seven words and initializes only the opaque base's texture.
/// Both projections must pass `TMD_GTE_ERROR_FLAG`. NCLIP(0,1,2) > 0 and
/// NCLIP(1,2,3) < 0 select the drawable halves: corner 0 collapses onto 1 when
/// only the second half faces, and corner 3 onto 2 when only the first faces.
/// Neither half facing rejects the pair. Original four-corner depths feed AVSZ4.
///
/// The shared layer/base material colours light each packet; below blend 4096,
/// the layer RGB interpolates with neutral grey using a twelve-fraction-bit weight.
/// Environment UVs use the possibly collapsed pixel positions + (160,120) minus
/// rotated normals scaled by GPF12 with `colorBlend >> 9`. U >= 256 rebases
/// by 128 and clamps to 191; negative U/V clamp to zero and V clamps to 239.
/// Each colour group's high byte temporarily records 0/1 page rebasing. Any
/// marker selects the additive direct-colour page at VRAM (576,256); unmarked
/// corners subtract 128 from U >= 128 and otherwise clamp to zero. Without
/// markers the page is (448,256). Commands 0x3E/0x3C complete the layer/base;
/// the layer is linked first and the base second at AVSZ4 depth.
///
/// `elements` begins after the three-word header. Initial `elemCount` is
/// 0..65535; `elemStride` counts u32 words. Full strides must be readable and
/// masked geometry references must select complete eight-byte SVECTORs in their
/// respective borrowed arrays; low-bit roles and array extents are unproven.
/// The caller supplies GTE transform/lighting and the relevant depth scale.
/// `primWrite` must provide two aligned 52-byte `POLY_GT4` slot(s) per element
/// in the selected half's second region. Packet capacities are unchecked.
///
/// Accepted packets prepend to the displaced OT at
/// `(((u32)OTZ << gDisplayState.otDepthShift) >> 4) & 1023`; normal shifts are
/// 0..3. Every bucket must fit its backing table. DMA links encode 24 address
/// bits; packet and OT storage must remain GPU-visible until consumption ends.
/// Advances `primWrite` by two slot(s) per element, including rejected ones,
/// and returns `elements + initial count * elemStride`, leaving the next marker
/// unconsumed. The count ends at -1 even for an empty record, which reads no
/// payload and advances neither cursor. `preXformWrite` is unchanged.
/// `objectFlags` is ignored, including reverse-culling flags. Storage is borrowed;
/// no pointer is retained.
u32* tmdDrawStreamPrimGt4EnvLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects and lights Gouraud textured triangles from one element material colour.
///
/// Draw resolution selects this callback for `0x30`. Words 0..2 pack three
/// vertex and three normal byte references; word 3 is the material RGB/code
/// word. Construction requires at least seven words and initializes the texture
/// suffix. The three normals light all corners from that single material.
/// `TMD_GTE_ERROR_FLAG` or NCLIP(0,1,2) <= 0 rejects the triangle. Accepted
/// packets receive nine payload words, command 0x34 (or 0x36 when the object's
/// `TMD_OBJECT_SEMI_TRANS` flag is set), and an AVSZ3 ordering-table link.
/// Texture fields persist; the callback argument `objectFlags` is ignored.
///
/// `elements` begins after the three-word header. Initial `elemCount` is
/// 0..65535; `elemStride` counts u32 words. Full strides must be readable and
/// masked geometry references must select complete eight-byte SVECTORs in their
/// respective borrowed arrays; low-bit roles and array extents are unproven.
/// The caller supplies GTE transform/lighting and the relevant depth scale.
/// `primWrite` must provide one aligned 40-byte `POLY_GT3` slot(s) per element
/// in the selected half's second region. Packet capacities are unchecked.
///
/// Accepted packets prepend to the displaced OT at
/// `(((u32)OTZ << gDisplayState.otDepthShift) >> 4) & 1023`; normal shifts are
/// 0..3. Every bucket must fit its backing table. DMA links encode 24 address
/// bits; packet and OT storage must remain GPU-visible until consumption ends.
/// Advances `primWrite` by one slot(s) per element, including rejected ones,
/// and returns `elements + initial count * elemStride`, leaving the next marker
/// unconsumed. The count ends at -1 even for an empty record, which reads no
/// payload and advances neither cursor. `preXformWrite` is unchanged.
/// `objectFlags` is ignored, including reverse-culling flags. Storage is borrowed;
/// no pointer is retained.
u32* tmdDrawStreamPrimGt3ElemColor(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects and lights a Gouraud textured quad from one element material colour.
///
/// Draw resolution selects this callback for `0x70`. Words 0..3 pack four
/// vertex u16 byte references followed by four normal references; word 4 is
/// the material RGB/code word used to light all four corners. Construction
/// requires at least eight words and seeds the texture suffix in words 5..7.
/// Masked geometry references must select complete eight-byte SVECTORs in the
/// borrowed arrays; their full extents and the low reference bits are unproven.
///
/// `TMD_GTE_ERROR_FLAG` rejects either projection. After both succeed, the quad
/// is accepted if NCLIP(0,1,2) > 0, or otherwise NCLIP(1,2,3) < 0. Earlier XY
/// stores can remain in rejected packets. Accepted packets receive all four
/// pixel XY and lit RGB values, twelve payload words and command 0x3C, or 0x3E
/// when the object's `TMD_OBJECT_SEMI_TRANS` is set. Texture fields persist.
/// `objectFlags` is ignored, including reverse-culling flags.
///
/// `elements` starts after the three-word header; count is 0..65535 and stride
/// counts u32 words. The caller supplies GTE projection/lighting and ZSF4, and
/// `primWrite` provides one aligned, writable 52-byte POLY_GT4 slot per element
/// in the second region. Each full element stride must be readable. Capacities
/// are unchecked. AVSZ4 selects `(((u32)OTZ << gDisplayState.otDepthShift) >> 4)
/// & 1023` in the displaced OT; normal shifts are 0..3 and buckets must fit it.
/// DMA links retain 24 address bits; packets and OT stay GPU-visible until used.
/// Advances `primWrite` by one slot even on rejection and returns the cursor
/// after count * stride words, leaving the next marker. Count ends at -1,
/// including on empty input. `preXformWrite` is unchanged. No storage is retained.
u32* tmdDrawStreamPrimGt4ElemColor(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects, culls and links opaque raw-texture triangles from TMD stream records.
///
/// Draw resolution selects this callback for `0x1C`. `elements` begins after
/// the three-word record header. Its first three u16 values encode vertex
/// byte offsets; masking with 0xFFF8 discards the low three bits and selects
/// complete eight-byte SVECTOR entries in `workspace->verts`. The meaning of
/// the discarded bits is unproven. The source array's extent is not supplied.
/// Drawing reads two words per element; `modelLightingStreamPrimFt3` needs
/// words 2..4 for persistent texture data, so a complete element needs at least
/// five words. `elemStride` counts u32 words and `elemCount` starts at 0..65535.
/// The payload must cover every full stride; bounds are unchecked.
///
/// The caller supplies the part's GTE transform and ZSF3. GTE FLAG bit 31 or
/// nonpositive NCLIP(0,1,2) rejects a triangle without changing its packet or
/// OT. Accepted triangles receive screen XY in pixels, seven payload words
/// and command 0x25 (opaque FT3, texture RGB bypasses colour modulation).
/// Texture and colour bytes are preserved; raw-texture drawing ignores RGB.
/// `objectFlags` is unused, including blend and reverse-culling flags.
///
/// `primWrite` must address one writable, word-aligned 32-byte POLY_FT3 slot
/// per element in the selected half's second region, with texture initialized.
/// Accepted packets prepend to `workspace->ot` using AVSZ3 at bucket
/// `(((u32)OTZ << gDisplayState.otDepthShift) >> 4) & 1023` (shift 0..3 in
/// normal drawing). The OT base includes the object's signed entry displacement;
/// every resulting bucket must fit its backing table. Links retain 24 address
/// bits. Packet and OT storage must remain GPU-visible until consumption ends.
///
/// Advances `primWrite` by one slot per element, including rejected triangles,
/// and returns `elements + initial elemCount * elemStride`, leaving the next
/// record or marker unconsumed. Consumes `elemCount` to -1 even for an empty
/// record, which reads no payload and advances neither cursor. GTE FLAG/result
/// scratch is updated for nonempty records; `preXformWrite` is unchanged. All
/// input storage is borrowed; no pointer is retained by the callback.
u32* tmdDrawStreamPrimFt3(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects, culls and links opaque raw-texture quads from TMD stream records.
///
/// Draw resolution selects this callback for `0x5C`. `elements` begins after
/// the three-word record header. Words 0..1 pack four u16 vertex byte offsets,
/// in corner order. Each is masked with 0xFFF8 and must select a complete
/// eight-byte SVECTOR in `workspace->verts`; discarded low-bit roles and the
/// array's full extent are unproven. Drawing reads two words per element;
/// `modelLightingStreamPrimFt4` initializes texture fields from words 2..4,
/// so full elements need at least five. `elemStride` counts u32 words and
/// `elemCount` starts at 0..65535. Every full stride must be readable.
///
/// The caller supplies the part's GTE transform and ZSF4. Projects corners
/// 0,1,2 together, stores their XY after a successful projection, then projects
/// corner 3. Either projection's GTE FLAG bit 31 rejects the quad. Facing keeps
/// NCLIP(0,1,2) > 0 or, if that fails, NCLIP(1,2,3) < 0. Rejected quads may
/// retain the first three XY writes; they are not linked. Accepted quads receive
/// corner 3's XY, nine payload words and command 0x2D (opaque FT4, texture RGB
/// bypasses colour modulation). Texture and RGB bytes persist; drawing ignores
/// RGB. `objectFlags` is unused, including blend and reverse-culling flags.
///
/// `primWrite` must provide one writable, word-aligned 40-byte POLY_FT4 slot
/// per element in the selected half's second region, with texture initialized.
/// Accepted packets prepend to `workspace->ot` using AVSZ4 at bucket
/// `(((u32)OTZ << gDisplayState.otDepthShift) >> 4) & 1023` (shift 0..3 in
/// normal drawing). The OT base includes the object's signed entry displacement;
/// each bucket must fit the backing table. Links retain 24 address bits. Bounds
/// are unchecked; packet and OT storage must outlive GPU consumption.
///
/// Advances `primWrite` by one slot per element, including rejected quads,
/// and returns `elements + initial elemCount * elemStride`, leaving the next
/// record or marker unconsumed. Consumes `elemCount` to -1 even for an empty
/// record, which reads no payload and advances neither cursor. GTE FLAG/result
/// scratch is updated for nonempty records; `preXformWrite` is unchanged.
/// Workspace, stream and vertices are borrowed; no pointer is retained.
u32* tmdDrawStreamPrimFt4(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects, culls and links prebuilt unlit semitransparent Gouraud textured quads.
///
/// Draw resolution selects this callback for `0x156`. Words 0..1 pack four
/// vertex byte references. `tmdBuildStreamGt4Unlit` requires at least nine words
/// and seeds all four RGB words, texture fields, twelve-word length and command
/// 0x3E; this callback preserves them and uses no normals or lighting. Either
/// projection's `TMD_GTE_ERROR_FLAG` rejects the quad. Facing accepts
/// NCLIP(0,1,2) > 0 or NCLIP(1,2,3) < 0. The first three pixel XY pairs are
/// stored before projecting corner 3, so a later rejection can leave partial XY.
/// Accepted quads receive corner 3 XY and an AVSZ4 ordering-table link.
///
/// `elements` begins after the three-word header. Initial `elemCount` is
/// 0..65535; `elemStride` counts u32 words. Full strides must be readable and
/// masked geometry references must select complete eight-byte SVECTORs in their
/// respective borrowed arrays; low-bit roles and array extents are unproven.
/// The caller supplies GTE transform/lighting and the relevant depth scale.
/// `primWrite` must provide one aligned 52-byte `POLY_GT4` slot(s) per element
/// in the selected half's second region. Packet capacities are unchecked.
///
/// Accepted packets prepend to the displaced OT at
/// `(((u32)OTZ << gDisplayState.otDepthShift) >> 4) & 1023`; normal shifts are
/// 0..3. Every bucket must fit its backing table. DMA links encode 24 address
/// bits; packet and OT storage must remain GPU-visible until consumption ends.
/// Advances `primWrite` by one slot(s) per element, including rejected ones,
/// and returns `elements + initial count * elemStride`, leaving the next marker
/// unconsumed. The count ends at -1 even for an empty record, which reads no
/// payload and advances neither cursor. `preXformWrite` is unchanged.
/// `objectFlags` is ignored, including reverse-culling flags. Storage is borrowed;
/// no pointer is retained.
u32* tmdDrawStreamPrimGt4Unlit(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects, culls and links flat-coloured quads from TMD stream records.
///
/// Draw resolution selects this callback for `0x44`. Words 0..1 pack four
/// vertex byte references; construction requires three words and seeds RGB.
/// Either projection's `TMD_GTE_ERROR_FLAG` rejects the quad. Facing accepts
/// NCLIP(0,1,2) > 0 or NCLIP(1,2,3) < 0. First-three-corner pixel XY is
/// stored before corner 3's projection, so later rejection can leave partial XY.
/// Accepted packets receive corner 3 XY, five payload words, opaque command
/// 0x28 and an AVSZ4 ordering-table link. Material RGB persists.
///
/// `elements` begins after the three-word header. Initial `elemCount` is
/// 0..65535; `elemStride` counts u32 words. Full strides must be readable and
/// masked geometry references must select complete eight-byte SVECTORs in their
/// respective borrowed arrays; low-bit roles and array extents are unproven.
/// The caller supplies GTE transform/lighting and the relevant depth scale.
/// `primWrite` must provide one aligned 24-byte `POLY_F4` slot(s) per element
/// in the selected half's second region. Packet capacities are unchecked.
///
/// Accepted packets prepend to the displaced OT at
/// `(((u32)OTZ << gDisplayState.otDepthShift) >> 4) & 1023`; normal shifts are
/// 0..3. Every bucket must fit its backing table. DMA links encode 24 address
/// bits; packet and OT storage must remain GPU-visible until consumption ends.
/// Advances `primWrite` by one slot(s) per element, including rejected ones,
/// and returns `elements + initial count * elemStride`, leaving the next marker
/// unconsumed. The count ends at -1 even for an empty record, which reads no
/// payload and advances neither cursor. `preXformWrite` is unchanged.
/// `objectFlags` is ignored, including reverse-culling flags. Storage is borrowed;
/// no pointer is retained.
u32* tmdDrawStreamPrimF4(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects, culls and links flat-coloured triangles from TMD stream records.
///
/// Draw resolution selects this callback for `0x04`. The first three u16 values
/// are vertex byte references; the fourth is unread. Construction requires
/// three words and seeds the material RGB. `TMD_GTE_ERROR_FLAG` and
/// NCLIP(0,1,2) <= 0 reject the triangle; FLAG is checked again after the
/// accepted facing test and XY store. Accepted packets receive four payload
/// words, opaque command 0x20 and an AVSZ3 ordering-table link. RGB persists.
///
/// `elements` begins after the three-word header. Initial `elemCount` is
/// 0..65535; `elemStride` counts u32 words. Full strides must be readable and
/// masked geometry references must select complete eight-byte SVECTORs in their
/// respective borrowed arrays; low-bit roles and array extents are unproven.
/// The caller supplies GTE transform/lighting and the relevant depth scale.
/// `primWrite` must provide one aligned 20-byte `POLY_F3` slot(s) per element
/// in the selected half's second region. Packet capacities are unchecked.
///
/// Accepted packets prepend to the displaced OT at
/// `(((u32)OTZ << gDisplayState.otDepthShift) >> 4) & 1023`; normal shifts are
/// 0..3. Every bucket must fit its backing table. DMA links encode 24 address
/// bits; packet and OT storage must remain GPU-visible until consumption ends.
/// Advances `primWrite` by one slot(s) per element, including rejected ones,
/// and returns `elements + initial count * elemStride`, leaving the next marker
/// unconsumed. The count ends at -1 even for an empty record, which reads no
/// payload and advances neither cursor. `preXformWrite` is unchanged.
/// `objectFlags` is ignored, including reverse-culling flags. Storage is borrowed;
/// no pointer is retained.
u32* tmdDrawStreamPrimF3(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects, culls and links semitransparent raw-texture triangles.
///
/// Draw resolution selects this callback for `0x1E`. The first three u16 values
/// are vertex byte references; the fourth is unread. Construction requires five
/// words and initializes texture fields. `TMD_GTE_ERROR_FLAG` or
/// NCLIP(0,1,2) <= 0 rejects the triangle without packet or OT writes.
/// Accepted packets receive pixel XY, seven payload words, command 0x27
/// (semitransparent FT3, texture RGB bypasses colour modulation), and an AVSZ3
/// ordering-table link. Texture/RGB bytes persist; raw drawing ignores RGB.
///
/// `elements` begins after the three-word header. Initial `elemCount` is
/// 0..65535; `elemStride` counts u32 words. Full strides must be readable and
/// masked geometry references must select complete eight-byte SVECTORs in their
/// respective borrowed arrays; low-bit roles and array extents are unproven.
/// The caller supplies GTE transform/lighting and the relevant depth scale.
/// `primWrite` must provide one aligned 32-byte `POLY_FT3` slot(s) per element
/// in the selected half's second region. Packet capacities are unchecked.
///
/// Accepted packets prepend to the displaced OT at
/// `(((u32)OTZ << gDisplayState.otDepthShift) >> 4) & 1023`; normal shifts are
/// 0..3. Every bucket must fit its backing table. DMA links encode 24 address
/// bits; packet and OT storage must remain GPU-visible until consumption ends.
/// Advances `primWrite` by one slot(s) per element, including rejected ones,
/// and returns `elements + initial count * elemStride`, leaving the next marker
/// unconsumed. The count ends at -1 even for an empty record, which reads no
/// payload and advances neither cursor. `preXformWrite` is unchanged.
/// `objectFlags` is ignored, including reverse-culling flags. Storage is borrowed;
/// no pointer is retained.
u32* tmdDrawStreamPrimFt3SemiTrans(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects, culls and links semitransparent raw-texture quads.
///
/// Draw resolution selects this callback for `0x5E`. Words 0..1 pack four
/// vertex byte references; construction requires five words and initializes
/// texture fields. Either projection's `TMD_GTE_ERROR_FLAG` rejects the quad.
/// Facing accepts NCLIP(0,1,2) > 0 or NCLIP(1,2,3) < 0. First-three-corner
/// pixel XY is stored before projecting corner 3, so later rejection can leave
/// partial XY. Accepted packets receive corner 3 XY, nine payload words,
/// command 0x2F (semitransparent FT4, texture RGB bypasses colour modulation),
/// and an AVSZ4 ordering-table link. Texture/RGB persists; raw drawing ignores RGB.
///
/// `elements` begins after the three-word header. Initial `elemCount` is
/// 0..65535; `elemStride` counts u32 words. Full strides must be readable and
/// masked geometry references must select complete eight-byte SVECTORs in their
/// respective borrowed arrays; low-bit roles and array extents are unproven.
/// The caller supplies GTE transform/lighting and the relevant depth scale.
/// `primWrite` must provide one aligned 40-byte `POLY_FT4` slot(s) per element
/// in the selected half's second region. Packet capacities are unchecked.
///
/// Accepted packets prepend to the displaced OT at
/// `(((u32)OTZ << gDisplayState.otDepthShift) >> 4) & 1023`; normal shifts are
/// 0..3. Every bucket must fit its backing table. DMA links encode 24 address
/// bits; packet and OT storage must remain GPU-visible until consumption ends.
/// Advances `primWrite` by one slot(s) per element, including rejected ones,
/// and returns `elements + initial count * elemStride`, leaving the next marker
/// unconsumed. The count ends at -1 even for an empty record, which reads no
/// payload and advances neither cursor. `preXformWrite` is unchanged.
/// `objectFlags` is ignored, including reverse-culling flags. Storage is borrowed;
/// no pointer is retained.
u32* tmdDrawStreamPrimFt4SemiTrans(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects and lights opaque Gouraud triangles from a material colour per corner.
///
/// Draw resolution selects this callback for `0x120`. Words 0..2 pack three
/// vertex and three normal byte references; words 3..5 supply RGB/code words
/// for the respective corners. Each corner is lit independently from its material
/// and normal. `TMD_GTE_ERROR_FLAG` or NCLIP(0,1,2) <= 0 rejects the triangle.
/// Accepted packets receive pixel XY, all corner RGB, six payload words, command
/// 0x30 and an AVSZ3 ordering-table link. Six words is the minimum readable
/// extent. Construction reserves one G3 slot per `0x120` element.
///
/// `elements` begins after the three-word header. Initial `elemCount` is
/// 0..65535; `elemStride` counts u32 words. Full strides must be readable and
/// masked geometry references must select complete eight-byte SVECTORs in their
/// respective borrowed arrays; low-bit roles and array extents are unproven.
/// The caller supplies GTE transform/lighting and the relevant depth scale.
/// `primWrite` must provide one aligned 28-byte `POLY_G3` slot(s) per element
/// in the selected half's second region. Packet capacities are unchecked.
///
/// Accepted packets prepend to the displaced OT at
/// `(((u32)OTZ << gDisplayState.otDepthShift) >> 4) & 1023`; normal shifts are
/// 0..3. Every bucket must fit its backing table. DMA links encode 24 address
/// bits; packet and OT storage must remain GPU-visible until consumption ends.
/// Advances `primWrite` by one slot(s) per element, including rejected ones,
/// and returns `elements + initial count * elemStride`, leaving the next marker
/// unconsumed. The count ends at -1 even for an empty record, which reads no
/// payload and advances neither cursor. `preXformWrite` is unchanged.
/// `objectFlags` is ignored, including reverse-culling flags. Storage is borrowed;
/// no pointer is retained.
u32* tmdDrawStreamPrimG3CornerColors(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects and lights semitransparent Gouraud triangles from a material colour per corner.
///
/// Draw resolution selects this callback for `0x122`. Words 0..2 pack three
/// vertex and three normal byte references; words 3..5 supply the respective
/// corner RGB/code words. Each corner is lit independently. Projection under
/// `TMD_GTE_ERROR_FLAG` or NCLIP(0,1,2) <= 0 rejects the triangle. Accepted
/// packets receive pixel XY, all corner RGB, six payload words, command 0x32
/// and an AVSZ3 ordering-table link. Six words is the minimum readable extent.
/// The construction dispatcher currently skips `0x122`; drawing nevertheless
/// requires and consumes one writable G3 slot per element.
///
/// `elements` begins after the three-word header. Initial `elemCount` is
/// 0..65535; `elemStride` counts u32 words. Full strides must be readable and
/// masked geometry references must select complete eight-byte SVECTORs in their
/// respective borrowed arrays; low-bit roles and array extents are unproven.
/// The caller supplies GTE transform/lighting and the relevant depth scale.
/// `primWrite` must provide one aligned 28-byte `POLY_G3` slot(s) per element
/// in the selected half's second region. Packet capacities are unchecked.
///
/// Accepted packets prepend to the displaced OT at
/// `(((u32)OTZ << gDisplayState.otDepthShift) >> 4) & 1023`; normal shifts are
/// 0..3. Every bucket must fit its backing table. DMA links encode 24 address
/// bits; packet and OT storage must remain GPU-visible until consumption ends.
/// Advances `primWrite` by one slot(s) per element, including rejected ones,
/// and returns `elements + initial count * elemStride`, leaving the next marker
/// unconsumed. The count ends at -1 even for an empty record, which reads no
/// payload and advances neither cursor. `preXformWrite` is unchanged.
/// `objectFlags` is ignored, including reverse-culling flags. Storage is borrowed;
/// no pointer is retained.
u32* tmdDrawStreamPrimG3CornerColorsSemiTrans(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects and lights opaque Gouraud quads from a material colour per corner.
///
/// Draw resolution selects this callback for `0x160`. Words 0..3 pack four
/// vertex and four normal byte references; words 4..7 supply the respective
/// corner RGB/code words. Eight words is the minimum readable extent. Both
/// projections must pass `TMD_GTE_ERROR_FLAG`. Facing accepts NCLIP(0,1,2)
/// > 0 or NCLIP(1,2,3) < 0. First-three-corner XY is stored before corner 3's
/// projection, so rejection can leave partial XY. Accepted packets receive all
/// corner pixel XY and independently lit RGB, eight payload words, command
/// 0x38 and an AVSZ4 ordering-table link. Construction reserves one G4 slot.
///
/// `elements` begins after the three-word header. Initial `elemCount` is
/// 0..65535; `elemStride` counts u32 words. Full strides must be readable and
/// masked geometry references must select complete eight-byte SVECTORs in their
/// respective borrowed arrays; low-bit roles and array extents are unproven.
/// The caller supplies GTE transform/lighting and the relevant depth scale.
/// `primWrite` must provide one aligned 36-byte `POLY_G4` slot(s) per element
/// in the selected half's second region. Packet capacities are unchecked.
///
/// Accepted packets prepend to the displaced OT at
/// `(((u32)OTZ << gDisplayState.otDepthShift) >> 4) & 1023`; normal shifts are
/// 0..3. Every bucket must fit its backing table. DMA links encode 24 address
/// bits; packet and OT storage must remain GPU-visible until consumption ends.
/// Advances `primWrite` by one slot(s) per element, including rejected ones,
/// and returns `elements + initial count * elemStride`, leaving the next marker
/// unconsumed. The count ends at -1 even for an empty record, which reads no
/// payload and advances neither cursor. `preXformWrite` is unchanged.
/// `objectFlags` is ignored, including reverse-culling flags. Storage is borrowed;
/// no pointer is retained.
u32* tmdDrawStreamPrimG4CornerColors(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects and lights semitransparent Gouraud quads from a material colour per corner.
///
/// Draw resolution selects this callback for `0x162`. Words 0..3 pack four
/// vertex and four normal byte references; words 4..7 supply the respective
/// corner RGB/code words. Eight words is the minimum readable extent.
/// `TMD_GTE_ERROR_FLAG` rejects either projection. Only positive first-triangle
/// NCLIP(0,1,2) facing is drawable: corner 3 is projected after this gate, and
/// the saved positive facing result takes the draw arm. A later rejection can
/// leave first-three-corner XY writes. Accepted packets receive all corner pixel
/// XY and independently lit RGB, eight payload words, command 0x3A and an
/// AVSZ4 ordering-table link. Construction currently skips `0x162`; drawing
/// nevertheless requires and consumes one writable G4 slot per element.
///
/// `elements` begins after the three-word header. Initial `elemCount` is
/// 0..65535; `elemStride` counts u32 words. Full strides must be readable and
/// masked geometry references must select complete eight-byte SVECTORs in their
/// respective borrowed arrays; low-bit roles and array extents are unproven.
/// The caller supplies GTE transform/lighting and the relevant depth scale.
/// `primWrite` must provide one aligned 36-byte `POLY_G4` slot(s) per element
/// in the selected half's second region. Packet capacities are unchecked.
///
/// Accepted packets prepend to the displaced OT at
/// `(((u32)OTZ << gDisplayState.otDepthShift) >> 4) & 1023`; normal shifts are
/// 0..3. Every bucket must fit its backing table. DMA links encode 24 address
/// bits; packet and OT storage must remain GPU-visible until consumption ends.
/// Advances `primWrite` by one slot(s) per element, including rejected ones,
/// and returns `elements + initial count * elemStride`, leaving the next marker
/// unconsumed. The count ends at -1 even for an empty record, which reads no
/// payload and advances neither cursor. `preXformWrite` is unchanged.
/// `objectFlags` is ignored, including reverse-culling flags. Storage is borrowed;
/// no pointer is retained.
u32* tmdDrawStreamPrimG4CornerColorsSemiTrans(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects unlit stream vertices into packet XY fields and the depth cache.
///
/// Draw resolution selects this callback for `0xC4`. Each element supplies a
/// u16 vertex byte reference and a u16 byte destination offset in its first
/// word; later words are unread. The vertex offset masked with 0xFFF8 must
/// select a complete eight-byte SVECTOR and the unmasked reference >> 3 must
/// fit the 1024-entry depth cache. Consecutive identical references reuse the
/// current GTE screen XY and cached screen Z. Each destination is relative to
/// `preXformWrite` and must provide an aligned writable four-byte pixel XY word
/// in the first packet region. Colours, texture fields and normals are untouched.
///
/// The handler does not save hardware FLAG after RTPS: `TMD_GTE_ERROR_FLAG` in
/// the incoming `workspace->gteFlag` marks every newly cached screen Z with
/// `TMD_VERTEX_DEPTH_INVALID`, preserving its low sixteen bits. The caller must
/// supply that saved word as well as GTE projection and the borrowed geometry,
/// depth cache and packet region. Normal draw setup does not initialize this
/// word; which earlier command's decision this opcode should inherit is unproven.
///
/// `elements` starts after the three-word header; count is 0..65535 and stride
/// counts u32 words, at least one for a nonempty record. Full strides and all
/// destination/geometry extents must be valid; bounds are unchecked. Returns
/// the cursor after count * stride words without advancing either packet cursor.
/// Zero count stays zero; positive count ends at -1. `objectFlags` is ignored;
/// no storage is retained.
u32* tmdXformStreamVertsUnlit(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes persistent texture fields for a record of pre-transformed Gouraud textured triangles.
///
/// Construction selects this callback for `0x31`, `0x39`, `0x3B`, `0x131` and
/// `0x8039`. `elements` begins after the three-word record header; the caller
/// supplies `workspace->elemCount` (0..65535) and `elemStride` in u32 words,
/// at least five for a nonempty record. The first three u16 values are the
/// draw pass's byte offsets into its depth cache; word 1's high half is ignored.
/// Words 2 and 3 pack unsigned byte U/V texel coordinates with encoded CLUT and
/// texture-page settings. Only word 4's low half supplies U2/V2. Five words is
/// a minimum readable extent, not a fixed element size; bounds are unchecked.
///
/// `preXformWrite` must address one writable, four-byte-aligned `POLY_GT3` slot
/// per element in the selected buffer half's first region. The workspace supplies
/// signed encoded-address displacements: `texturePageOffset` (-128..127) and
/// `encodedClutOffset` (-8192..8128, 64 per palette row). Sums wrap modulo 65536
/// in the packet's u16 fields. Projection records supply screen positions and
/// colours during drawing; the primitive draw handler supplies length/code and
/// links. Construction preserves those fields and the SDK pad fields, including
/// `pad2` beside U2/V2, and leaves the second-region cursor `primWrite` unchanged.
///
/// Advances `preXformWrite` by one 40-byte packet per element and returns
/// `elements` advanced by the original count times the word stride. Consumes
/// `elemCount` to -1 even for an empty record, which reads no payload and advances
/// neither cursor. `objectFlags` is the shared callback argument, passed as zero
/// during construction and ignored here. The stream and packet region must
/// contain all elements and slots. All storage is borrowed; no pointer is retained.
u32* tmdBuildStreamGt3PreXform(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes persistent texture fields for pre-transformed Gouraud textured quads.
///
/// Construction selects this callback for `0x71`, `0x79`, `0x7B`, `0x171`
/// and `0x8079`. `elements` begins after the three-word record header. Words
/// 0..1 contain four depth-cache byte references used during drawing. Words 2
/// and 3 pack U0/V0 with encoded CLUT and U1/V1 with texture-page settings;
/// word 4 packs U2/V2 in its low half and U3/V3 in its high half. UV coordinates
/// are unsigned texel bytes. `elemStride` is in u32 words, at least five for
/// nonempty records; the payload must cover every full stride.
///
/// `preXformWrite` must provide one writable, word-aligned 52-byte POLY_GT4
/// slot per element within the selected half's first region. Signed workspace
/// `texturePageOffset` (-128..127 encoded units) and `encodedClutOffset`
/// (-8192..8128, 64 per palette row) relocate the copied addresses, wrapping
/// to u16. Projection commands supply positions and colours later; drawing
/// sets length/code and links. Construction preserves those fields and all
/// SDK pads, including the halfwords beside U2/V2 and U3/V3.
///
/// The caller supplies `elemCount` (0..65535). Advances `preXformWrite` by
/// one packet per element and returns `elements + initial count * elemStride`,
/// leaving the following marker unconsumed. Consumes the count to -1 even for
/// an empty record, which reads no payload and advances neither cursor.
/// `primWrite` is unchanged. `objectFlags` is passed as zero and ignored.
/// Capacities are unchecked. All storage is borrowed; no pointer is retained.
u32* tmdBuildStreamGt4PreXform(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Handler of a stream's pre-transformed flat-quad records (`0x45`): each
/// element contributes one untextured, unlit quad to the buffer half's first
/// region, with the element's colour word written into it.
///
/// The opcode's `0x01` bit marks the corners as already in screen space. This
/// command writes the packet's fixed fields: its length (5 words after the
/// tag), the element's colour and the opaque flat-quad code (`0x28`). The
/// colour is the element's third word and includes the command byte, so the
/// code is stored after it. The record's draw handler
/// (`tmdDrawStreamPrimF4PreXform`) reads four depth-cache offsets from the
/// element's leading halfwords, culls from the screen coordinates already
/// stored in the packet, and links a quad the cull accepts. That handler
/// leaves this length, code and colour unchanged.
///
/// `preXformWrite` advances by one quad per element. The record has no
/// variant for `flags` to select, and packet construction passes zero, so
/// `flags` goes unread. The returned cursor is the stream advanced by one
/// element stride per element.
u32* modelLightingStreamPrimF4PreXform(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Handler of a stream's pre-transformed flat-triangle records (`0x5`): each
/// element contributes one untextured, unlit triangle to the buffer half's
/// first region, with the element's colour word written into it.
///
/// The opcode's `0x01` bit marks the corners as already in screen space. This
/// command writes the packet's fixed fields: its length (4 words after the
/// tag), the element's colour and the opaque flat-triangle code (`0x20`). The
/// colour is the element's third word and includes the command byte, so the
/// code is stored after it. The record's draw handler reads three depth-cache
/// offsets from the element's leading halfwords, culls from the screen
/// coordinates already stored in the packet, and links a triangle the cull
/// accepts. That handler leaves this length, code and colour unchanged.
///
/// `preXformWrite` advances by one triangle per element. The record has no
/// variant for `flags` to select, and packet construction passes zero, so
/// `flags` goes unread. The returned cursor is the stream advanced by one
/// element stride per element.
u32* modelLightingStreamPrimF3PreXform(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Initializes persistent texture fields for a record of gouraud textured triangles.
///
/// Construction selects this callback for `0x38`, `0x3A`, `0x8038`, `0x10038`,
/// `0x1003A` and `0x20038`. `elements` starts after the three-word record header.
/// The caller supplies `workspace->elemCount` (0..65535) and `elemStride` in
/// u32 words, at least six per element. The first three words pack three vertex
/// and three normal references; words 3 and 4 pack unsigned byte U/V texel
/// coordinates with encoded CLUT and texture-page settings. Word 5's low half
/// packs U2/V2; its high half is ignored.
///
/// `primWrite` must address one writable, four-byte-aligned `POLY_GT3` slot per
/// element in the selected buffer half's second region. Texture page and CLUT
/// sums wrap in their u16 fields after adding the workspace's signed encoded
/// displacements: `texturePageOffset` (-128..127) and `encodedClutOffset`
/// (-8192..8128, 64 per palette row). Drawing supplies positions, colours, packet
/// lengths/codes and links later; construction preserves those fields and `pad2`.
///
/// Advances `primWrite` by one 40-byte packet per element and returns the cursor
/// advanced by `elemCount * elemStride` words. The count is consumed to -1 even
/// for an empty record. `objectFlags` is the shared callback argument, passed as
/// zero during construction and ignored here. Stream and packet capacities are
/// caller obligations; the workspace and storage are borrowed, with no pointer
/// retained by this callback.
u32* tmdBuildStreamGt3(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes persistent texture fields for a record of Gouraud textured quads.
///
/// Construction selects this callback for `0x78`, `0x7A`, `0x8078`, `0x10078`
/// and `0x20078`. `elements` starts after the three-word record header; the
/// caller supplies `workspace->elemCount` (0..65535) and `elemStride` in u32
/// words, at least seven per element. Words 0..3 pack four vertex then four
/// normal u16 references for drawing. Words 4/5 pack unsigned byte U/V texel
/// coordinates with encoded CLUT/texture-page settings; word 6 packs U2/V2
/// in its low half and U3/V3 in its high half. Seven words is a minimum readable
/// extent, not a fixed element size. Construction does not access geometry.
///
/// `primWrite` must address one writable, four-byte-aligned `POLY_GT4` slot per
/// element in the selected buffer half's second region. The workspace supplies
/// signed encoded-address displacements: `texturePageOffset` (-128..127) and
/// `encodedClutOffset` (-8192..8128, 64 per palette row). Sums wrap modulo
/// 65536 in the u16 packet fields. Drawing supplies positions, lit colours,
/// length/code and links later; construction preserves those fields and all
/// packet pad fields.
///
/// Advances `primWrite` by one 52-byte packet per element and returns `elements`
/// advanced by the original count times the word stride, leaving the next
/// record or marker unconsumed. Consumes `elemCount` to -1 even for an empty
/// record, which reads no payload and advances neither cursor. Other workspace
/// fields remain unchanged. `objectFlags` is the shared callback argument,
/// passed as zero during construction and ignored here. Stream and packet
/// capacities are unchecked caller obligations. All storage is borrowed for
/// this call; no pointer is retained by this callback.
u32* tmdBuildStreamGt4(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes persistent texture fields for Gouraud textured triangles with one material colour per element.
///
/// Construction selects this callback for opcode `0x30`. `elements` begins
/// after the three-word record header; `workspace->elemCount` supplies 0..65535
/// elements and `elemStride` their stride in u32 words, at least seven. The
/// first three words pack three vertex then three normal u16 byte offsets.
/// Word 3 supplies the draw pass's material colour. Words 4 and 5 pack unsigned
/// byte U/V texel coordinates with encoded CLUT and texture-page settings;
/// word 6's low half supplies U2/V2 and its high half is ignored.
///
/// `primWrite` must address one writable, four-byte-aligned `POLY_GT3` slot per
/// element in the selected buffer half's second region. The workspace supplies
/// signed encoded-address displacements: `texturePageOffset` (-128..127) and
/// `encodedClutOffset` (-8192..8128, 64 per palette row). Sums wrap in the u16
/// packet fields. Drawing supplies positions, lit colours, length/code and
/// links later; construction preserves those fields and all packet pad fields.
///
/// Advances `primWrite` by one 40-byte packet per element and returns the cursor
/// advanced by the original count times the word stride. Consumes `elemCount`
/// to -1 even for an empty record. `objectFlags` is the shared callback argument,
/// passed as zero during construction and ignored here. Stream and packet
/// capacities are caller obligations; seven words is a minimum readable extent,
/// not a fixed element size. All storage is borrowed; no pointer is retained.
u32* tmdBuildStreamGt3ElemColor(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes persistent texture fields for a record of gouraud textured triangles with per-corner colours.
///
/// Packet construction selects this callback for opcode `0x130`. `elements`
/// starts after the three-word record header; `workspace->elemCount` supplies
/// 0..65535 elements and `elemStride` their stride in u32 words, at least nine.
/// The first three words pack vertex 0..2 then normal 0..2 as u16 byte offsets;
/// words 3..5 carry the corners' material colours for drawing. Words 6 and 7
/// pack unsigned byte U/V texel coordinates with encoded CLUT and texture-page
/// settings; word 8's low half packs U2/V2 and its high half is ignored.
///
/// `primWrite` must address one writable, four-byte-aligned `POLY_GT3` slot per
/// element in the selected buffer half's second region. The workspace supplies
/// signed encoded-address displacements: `texturePageOffset` (-128..127) and
/// `encodedClutOffset` (-8192..8128, 64 per palette row). Sums wrap in the u16
/// packet fields. Drawing supplies positions, lit colours, length/code and
/// links later; construction preserves those fields and the packet's pad fields.
///
/// Advances `primWrite` by one 40-byte packet per element and returns the cursor
/// advanced by the original count times the word stride. Consumes `elemCount`
/// to -1 even for an empty record. `objectFlags` is the shared callback argument,
/// passed as zero during construction and ignored here. Stream and packet
/// capacities are caller obligations. All storage is borrowed for this call;
/// no pointer is retained.
u32* tmdBuildStreamGt3CornerColors(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes persistent texture fields for Gouraud textured quads with one material colour per element.
///
/// Construction selects this callback for opcode `0x70`. `elements` begins
/// after the three-word record header; `workspace->elemCount` supplies 0..65535
/// elements and `elemStride` their stride in u32 words, at least eight. Words
/// 0..3 pack four vertex then four normal u16 byte offsets. Word 4 supplies
/// the draw pass's material colour. Words 5 and 6 pack unsigned byte U/V texel
/// coordinates with encoded CLUT and texture-page settings; word 7 packs U2/V2
/// in its low half and U3/V3 in its high half. Eight words is a minimum readable
/// extent, not a fixed element size. Construction does not access geometry or
/// the colour word.
///
/// `primWrite` must address one writable, four-byte-aligned `POLY_GT4` slot per
/// element in the selected buffer half's second region. The workspace supplies
/// signed encoded-address displacements: `texturePageOffset` (-128..127) and
/// `encodedClutOffset` (-8192..8128, 64 per palette row). Sums wrap modulo
/// 65536 in the u16 packet fields. Drawing supplies positions, lit colours,
/// length/code and links later; construction preserves those fields and all
/// packet pad fields.
///
/// Advances `primWrite` by one 52-byte packet per element and returns `elements`
/// advanced by the original count times the word stride, leaving the next
/// record or marker unconsumed. Consumes `elemCount` to -1 even for an empty
/// record, which reads no payload and advances neither cursor. Other workspace
/// fields remain unchanged. `objectFlags` is the shared callback argument,
/// passed as zero during construction and ignored here. Stream and packet
/// capacities are unchecked caller obligations. All storage is borrowed for
/// this call; no pointer is retained by this callback.
u32* tmdBuildStreamGt4ElemColor(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes persistent texture fields for Gouraud textured quads with one material colour per corner.
///
/// Construction selects this callback for opcode `0x170`. `elements` starts
/// after the three-word record header; `workspace->elemCount` supplies 0..65535
/// elements and `elemStride` their stride in u32 words, at least eleven. Words
/// 0..3 pack four vertex then four normal u16 byte offsets. Words 4..7 carry
/// the corners' material RGB and command bytes for drawing. Words 8 and 9 pack
/// unsigned byte U/V texel coordinates with encoded CLUT and texture-page settings; word 10
/// packs U2/V2 in its low half and U3/V3 in its high half. Eleven words is a
/// minimum readable extent, not a fixed element size. Construction does not
/// access geometry or the colour words.
///
/// `primWrite` must address one writable, four-byte-aligned `POLY_GT4` slot per
/// element in the selected buffer half's second region. The workspace supplies
/// signed encoded-address displacements: `texturePageOffset` (-128..127) and
/// `encodedClutOffset` (-8192..8128, 64 per palette row). Sums wrap modulo
/// 65536 in the u16 packet fields. Drawing supplies positions, lit colours,
/// length/code and links later; construction preserves those fields and all
/// packet pad fields.
///
/// Advances `primWrite` by one 52-byte packet per element and returns `elements`
/// advanced by the original count times the word stride, leaving the next
/// record or marker unconsumed. Consumes `elemCount` to -1 even for an empty
/// record, which reads no payload and advances neither cursor. Other workspace
/// fields remain unchanged. `objectFlags` is the shared callback argument,
/// passed as zero during construction and ignored here. Stream and packet
/// capacities are unchecked caller obligations. All storage is borrowed for
/// this call; no pointer is retained by this callback.
u32* tmdBuildStreamGt4CornerColors(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes persistent texture fields for textured triangles lit from one face normal.
///
/// Construction selects this callback for opcodes `0x18` and `0x1A`. `elements`
/// begins after the three-word record header. The caller supplies
/// `workspace->elemCount` (0..65535) and `elemStride` in u32 words, at least five
/// per element. Words 0/1 pack three vertex u16 byte offsets then one face-normal
/// offset for drawing; construction does not read them or access geometry.
/// Words 2/3 pack unsigned byte U/V texel coordinates with encoded CLUT and
/// texture-page settings. Only word 4's low half supplies U2/V2; its high half
/// is ignored. Five words is a minimum readable extent, not a fixed element size.
///
/// `primWrite` must address one writable, four-byte-aligned, 40-byte `POLY_GT3`
/// slot per element in the selected buffer half's second region. The workspace
/// supplies signed encoded-address displacements: `texturePageOffset`
/// (-128..127) and `encodedClutOffset` (-8192..8128, 64 per palette row). Sums
/// wrap modulo 65536 in the u16 packet fields. Drawing supplies positions, lit
/// colours, length/code and links later; construction preserves those fields
/// and all packet pad fields, including the halfword after U2/V2.
///
/// Advances `primWrite` by one packet per element and returns `elements`
/// advanced by the original count times the word stride, leaving the next
/// record or marker unconsumed. Consumes `elemCount` to -1 even for an empty
/// record, which reads no payload and advances neither cursor. The first-region
/// cursor and other workspace fields remain unchanged. `objectFlags` is the
/// shared callback argument, passed as zero during construction and ignored
/// here. Stream and packet capacities are unchecked caller obligations. All
/// storage is borrowed for the call; no pointer is retained by this callback.
u32* tmdBuildStreamGt3OneNormal(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes persistent texture fields for one-normal Gouraud textured quads.
///
/// `tmdBuildBufferHalf` selects this construction callback for `0x58`/`0x5A`.
/// `elements` starts after the three-word record header. Words 0..1 pack four
/// vertex byte references; the whole of word 2 is the face normal's unsigned
/// byte offset into the normal array. Those references are not read here.
/// Words 3 and 4 pack unsigned byte U/V texel coordinates with encoded
/// CLUT and page settings; word 5 packs U2/V2 and U3/V3 in its low/high halves.
/// `elemStride` counts u32 words and must be at least six for a nonempty record.
/// This minimum readable extent does not establish a fixed element size.
///
/// `primWrite` must provide one writable, word-aligned 52-byte POLY_GT4 slot
/// per element within the selected half's second region. Adds workspace
/// `texturePageOffset` (-128..127 encoded units) and `encodedClutOffset`
/// (-8192..8128, 64 per palette row), wrapping both address sums to u16.
/// Drawing supplies positions, face-normal lighting, length/code and links.
/// Construction preserves those fields and all SDK pads beside UV pairs.
///
/// With initial `elemCount` 0..65535, advances `primWrite` by one packet per
/// element and returns `elements + initial count * elemStride`, leaving the
/// next record or marker unconsumed. Consumes the count to -1 even for an empty
/// record, which reads no payload and advances neither cursor. `preXformWrite`
/// is unchanged. `objectFlags` is passed as zero and ignored. The stream must
/// cover every full stride and the packet region every slot; capacities are
/// unchecked. All storage is borrowed; no pointer is retained.
u32* tmdBuildStreamGt4OneNormal(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes unlit Gouraud textured quads with element colours and fixed semitransparency.
///
/// Construction selects this callback for `0x156`. `elements` begins after
/// the three-word record header. Words 0..1 pack four vertex references for
/// drawing. Words 2..5 copy all four corner RGB/pad words; corner 0's high byte
/// is then replaced with command 0x3E, enabling semitransparent colour-modulated
/// GT4 drawing independently of object flags. The packet length is twelve
/// payload words. Words 6 and 7 pack U0/V0 with CLUT and U1/V1 with page
/// settings; word 8 packs U2/V2 and U3/V3 in its low/high halves. UV coordinates
/// are unsigned texel bytes. No normals or lighting are used.
///
/// `primWrite` must provide one writable, word-aligned 52-byte POLY_GT4 slot
/// per element within the selected half's second region. Copied GPU addresses
/// receive signed workspace `texturePageOffset` (-128..127 encoded units) and
/// `encodedClutOffset` (-8192..8128, 64 per palette row), with u16 wrapping.
/// UV pad halfwords and screen XY persist. Drawing projects/culls the vertices
/// and links accepted packets, leaving the prebuilt colours, length and code.
///
/// The caller supplies `elemCount` (0..65535) and `elemStride` in u32 words,
/// at least nine for a nonempty record. Every full stride must be readable and
/// every packet slot writable; bounds are unchecked. Advances `primWrite` by
/// one packet per element and returns `elements + initial count * elemStride`,
/// leaving the following record/marker unconsumed. Consumes the count to -1
/// even for an empty record, which reads no payload and advances neither cursor.
/// `preXformWrite` is unchanged; `objectFlags` is passed as zero and ignored.
/// All storage is borrowed; no pointer is retained by the callback.
u32* tmdBuildStreamGt4Unlit(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes persistent texture fields for flat textured-triangle stream records.
///
/// `tmdBuildBufferHalf` selects this construction callback for `0x1C` and `0x1E`.
/// Each element reserves one `POLY_FT3` at `workspace->primWrite` in the selected
/// buffer half's second region. Drawing supplies the positions, packet length,
/// raw-texture command and ordering-table link; construction sets no colour.
/// `objectFlags` is the shared callback argument and is ignored here.
///
/// `elements` points past the three-word record header. Element words 2 and 3
/// pack unsigned byte U/V coordinates with encoded CLUT and texture-page words;
/// word 4's low half packs the third U/V pair. The signed encoded-page and CLUT
/// displacements in the workspace are added to the copied u16 fields, wrapping
/// to 16 bits. A CLUT displacement of 64 moves one palette row.
///
/// The initial `elemCount` is 0..65535 and `elemStride` is in u32 words, at least
/// five for nonempty records. The payload must cover every element stride, and
/// the four-byte-aligned destination must have room for `elemCount` packets
/// within its region. Both are borrowed for the call. Advances `primWrite` by
/// that many packets, leaves `elemCount` at -1 even for an empty record, and
/// returns the word after the payload without consuming a terminator.
u32* modelLightingStreamPrimFt3(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes persistent texture fields for flat textured-quad stream records.
///
/// `tmdBuildBufferHalf` selects this construction callback for `0x5C` and `0x5E`.
/// Each element reserves one `POLY_FT4` at `workspace->primWrite` in the selected
/// buffer half's second region. Drawing supplies screen positions, packet
/// length, the raw-texture command (opaque or semi-transparent) and the
/// ordering-table link. Construction sets no colour or packet pad fields.
/// `objectFlags` is the shared callback argument and is ignored here.
///
/// `elements` points past the three-word record header. Its first two words
/// carry four vertex references. Words 2 and 3 pack unsigned byte U/V texel
/// coordinates with encoded CLUT and texture-page settings; word 4 packs the
/// third and fourth U/V pairs in its low and high halves. The workspace's
/// signed encoded-page and CLUT displacements are added to the copied u16
/// fields, wrapping to 16 bits. One CLUT row contributes 64 encoded units.
///
/// The initial `elemCount` is 0..65535; `elemStride` is in u32 words and must be
/// at least five for nonempty records. The payload must cover every element
/// stride, and the four-byte-aligned destination must have room for `elemCount`
/// packets within its region. Workspace, payload and packets are borrowed for
/// the call. Advances `primWrite` by that many packets, leaves `elemCount` at
/// -1 even for an empty record, and returns the word after the payload without
/// consuming a terminator.
u32* modelLightingStreamPrimFt4(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Handler of a stream's flat-quad records (`0x44`): each element contributes
/// one untextured, unlit quad to the buffer half's second region, with the
/// element's colour word written into it.
///
/// The record is not pre-transformed, so its quad is built in the region the
/// draw pass transforms. This command writes the packet's fixed fields: its
/// length (5 words after the tag), the element's colour and the opaque
/// flat-quad code (`0x28`). The colour is the element's third word and
/// includes the command byte, so the code is stored after it. The draw pass
/// reads four vertex references from the element's leading halfwords, and
/// writes the length and code again on each quad it links. The linked quad
/// keeps this colour.
///
/// `primWrite` advances by one quad per element. The record has no variant
/// for `flags` to select, and packet construction passes zero, so `flags` goes
/// unread. The returned cursor is the stream advanced by one element stride per
/// element.
u32* modelLightingStreamPrimF4(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Handler of a stream's flat-triangle records (`0x4`): each element contributes
/// one untextured, unlit triangle to the buffer half's second region, with the
/// element's colour word written into it.
///
/// The record is not pre-transformed, so its triangle is built in the region the
/// draw pass transforms. This command writes the packet's fixed fields: its
/// length (4 words after the tag), the element's colour and the opaque
/// flat-triangle code (`0x20`). The colour is the element's third word and
/// includes the command byte, so the code is stored after it. The draw pass
/// reads three vertex references from the element's leading halfwords, and
/// writes the length and code again on each triangle it links. The linked
/// triangle keeps this colour.
///
/// `primWrite` advances by one triangle per element. The record has no variant
/// for `flags` to select, and packet construction passes zero, so `flags` goes
/// unread. The returned cursor is the stream advanced by one element stride per
/// element.
u32* modelLightingStreamPrimF3(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Initializes both textures in each layered Gouraud triangle packet pair.
///
/// `tmdBuildBufferHalf` selects this construction callback for `0x4038` in stage
/// 2, areas 15 and 16. `elements` starts after the three-word record header.
/// Words 0..2 pack three vertex and three normal references; words 3 and 4
/// pack unsigned byte U/V texel coordinates with encoded CLUT and texture-page
/// settings. Word 5's low half supplies U2/V2; its high half is ignored.
///
/// The first packet receives the element's texture fields plus the object's
/// independent `layerTexturePageOffset` (-128..127 encoded units) and signed
/// `layerClutRowOffset` (-128..127 palette rows, 64 encoded units per row).
/// The relocated page's ABR bit 5 is then set, preserving bit 6 and selecting
/// mode 1 or 3. The second packet instead adds the workspace's base
/// `texturePageOffset` (-128..127) and `encodedClutOffset` (-8192..8128).
/// All address sums wrap to u16; the base offsets are never added to the layer.
/// Drawing supplies the layer's semi-transparent command and the base's opaque
/// command, along with both packets' tags, positions and colours. Construction
/// preserves those fields and the SDK pad fields.
///
/// The caller supplies `elemCount` (0..65535) and `elemStride` in u32 words,
/// at least six for nonempty records, with every full stride readable.
/// `primWrite` must have two writable, four-byte-aligned `POLY_GT3` slots per
/// element within the selected buffer half's second region. Capacities are
/// unchecked. Workspace, object, payload and packet storage are borrowed for
/// the call; no pointer is retained. `objectFlags` is the shared callback
/// argument, passed as zero during construction and ignored here.
///
/// Advances `primWrite` by two 40-byte packets per element and returns
/// `elements + initial elemCount * elemStride`, leaving the next record or
/// terminator unconsumed. The count is consumed to -1 even for an empty record;
/// an empty record reads no element or object fields and advances neither cursor.
u32* tmdBuildStreamGt3OffsetLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes the opaque base texture in each layered gouraud triangle pair.
///
/// Construction selects this callback for `0x4038` outside the areas that use
/// `tmdBuildStreamGt3OffsetLayer`. `elements` starts after the three-word record
/// header. The caller supplies `workspace->elemCount` (0..65535) and `elemStride`
/// in u32 words, at least six per element. The first three words pack three
/// vertex and three normal references; words 3 and 4 pack unsigned byte U/V
/// texel coordinates with encoded CLUT and texture-page settings. Word 5's low
/// half packs U2/V2; its high half is ignored.
///
/// `primWrite` must address two writable, four-byte-aligned `POLY_GT3` slots per
/// element in the selected buffer half's second region. The first slot is left
/// untouched for the draw pass's semi-transparent environment layer. Only the
/// second slot's texture fields are initialized here: page and CLUT sums wrap
/// in their u16 fields after adding the workspace's signed encoded displacements,
/// `texturePageOffset` (-128..127) and `encodedClutOffset` (-8192..8128, 64 per
/// palette row). Its tag, colours/command, positions and pad fields are preserved
/// for drawing.
///
/// Advances `primWrite` by two 40-byte packets per element and returns the cursor
/// advanced by the initial `elemCount * elemStride` words, leaving any terminator
/// unconsumed. The count is consumed to -1 even for an empty record. `objectFlags`
/// is the shared callback argument, passed as zero during construction and ignored
/// here. The caller must provide the full element strides and packet capacity;
/// the workspace and storage are borrowed, with no pointer retained.
u32* tmdBuildStreamGt3LayeredBase(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes both textures in each layered Gouraud quad packet pair.
///
/// `tmdBuildBufferHalf` selects this construction callback for `0x4078` in stage
/// 2, areas 15 and 16. `elements` starts after the three-word record header.
/// Words 0..3 pack four vertex and four normal references; this callback does
/// not read them. Words 4 and 5 pack unsigned byte U/V texel coordinates with
/// encoded CLUT and texture-page settings. Word 6 packs U2/V2 in its low half
/// and U3/V3 in its high half.
///
/// The first packet receives the element's texture fields plus the object's
/// independent `layerTexturePageOffset` (-128..127 encoded units) and signed
/// `layerClutRowOffset` (-128..127 palette rows, 64 encoded units per row).
/// The relocated page's ABR bit 5 is then set, preserving bit 6 and selecting
/// mode 1 or 3. The second packet instead adds the workspace's base
/// `texturePageOffset` (-128..127) and `encodedClutOffset` (-8192..8128).
/// All address sums wrap to u16; the base offsets are never added to the layer.
/// Drawing supplies the layer's semi-transparent command and the base's opaque
/// command, along with both packets' tags, positions and colours. Construction
/// preserves those fields and the SDK pad fields.
///
/// The caller supplies `elemCount` (0..65535) and `elemStride` in u32 words,
/// at least seven for nonempty records, with every full stride readable.
/// `primWrite` must have two writable, four-byte-aligned `POLY_GT4` slots per
/// element within the selected buffer half's second region. Capacities are
/// unchecked. Workspace, object, payload and packet storage are borrowed for
/// the call; no pointer is retained. `objectFlags` is the shared callback
/// argument, passed as zero during construction and ignored here.
///
/// Advances `primWrite` by two 52-byte packets per element and returns
/// `elements + initial elemCount * elemStride`, leaving the next record or
/// terminator unconsumed. The count is consumed to -1 even for an empty record;
/// an empty record reads no element or object fields and advances neither cursor.
u32* tmdBuildStreamGt4OffsetLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes the opaque base texture in each layered Gouraud quad packet pair.
///
/// `tmdBuildBufferHalf` selects this construction callback for `0x4078` outside
/// stage 2, areas 15 and 16, which use `tmdBuildStreamGt4OffsetLayer` instead.
/// `elements` starts after the three-word record header. Words 0..3 pack four
/// vertex and four normal byte-offset references used during drawing; this
/// callback does not read them. Words 4 and 5 pack unsigned byte U/V texel
/// coordinates with encoded CLUT and texture-page settings. Word 6 packs U2/V2
/// in its low half and U3/V3 in its high half.
///
/// `primWrite` addresses two writable, four-byte-aligned `POLY_GT4` slots per
/// element in the selected buffer half's second region. The first slot is left
/// untouched for drawing to supply the semi-transparent environment layer's
/// texture. Only the second slot's texture fields are initialized here: the
/// page and CLUT sums wrap to u16 after adding the workspace's signed encoded
/// displacements, `texturePageOffset` (-128..127) and `encodedClutOffset`
/// (-8192..8128, 64 per palette row). Its tag, colours/command, screen positions
/// and SDK pad fields are preserved for drawing.
///
/// The caller supplies `elemCount` (0..65535), `elemStride` in u32 words (at
/// least seven for nonempty records), the full readable element strides and
/// packet capacity; capacities are unchecked. Workspace, payload and packet
/// storage are borrowed for the call; no storage is allocated. `objectFlags`
/// is the shared callback argument, passed as zero during construction and
/// ignored here.
///
/// Advances `primWrite` by two 52-byte packets per element and returns
/// `elements + initial elemCount * elemStride`, leaving the next record or
/// terminator unconsumed. The count is consumed to -1 even for an empty record;
/// an empty record reads no payload and advances neither cursor.
u32* tmdBuildStreamGt4LayeredBase(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes texture fields in pre-transformed environment/base triangle pairs.
///
/// `tmdBuildBufferHalf` selects this construction callback for `0x4039` outside
/// stage 2, areas 15 and 16. `elements` starts after the three-word record header.
/// Words 0..1 contain three u16 depth-cache references used during drawing;
/// their fourth halfword is ignored. Words 2 and 3 pack unsigned byte U/V texel
/// coordinates with encoded CLUT and texture-page settings. Word 4's low half
/// supplies U2/V2; its high half is ignored. These references are not read here.
///
/// The first packet is the environment layer, seeded with a 4-bit additive page
/// at VRAM word coordinates (960,256) and a CLUT at (256,240). Projection supplies
/// its U/V, positions and colours. `tmdDrawStreamPrimGt3PreXformEnvLayer` replaces
/// the page with a direct-colour environment page, where the retained CLUT is
/// unused. The second packet is the opaque base: its element texture receives
/// `texturePageOffset` (-128..127 encoded units) and `encodedClutOffset`
/// (-8192..8128, 64 per palette row). Address sums wrap to u16. Tags, commands,
/// colours, screen positions and SDK pad fields are preserved for drawing.
///
/// The caller supplies `elemCount` (0..65535) and `elemStride` in u32 words,
/// at least five for nonempty records, with every full stride readable.
/// `preXformWrite` must have two writable, four-byte-aligned `POLY_GT3` slots
/// per element within the selected buffer half's first region. Capacities are
/// unchecked. Workspace, payload and packet storage are borrowed for the call;
/// no pointer is retained. `objectFlags` is the shared callback argument,
/// passed as zero during construction and ignored here.
///
/// Advances `preXformWrite` by two 40-byte packets per element and returns
/// `elements + initial elemCount * elemStride`, leaving the next record or
/// terminator unconsumed. The count is consumed to -1 even for an empty record;
/// an empty record reads no payload and advances neither cursor.
u32* tmdBuildStreamGt3PreXformEnvLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes texture fields in pre-transformed environment/base quad pairs.
///
/// Construction selects this callback for `0x4079` outside stage 2, areas
/// 15 and 16. `elements` starts after the three-word record header. Words 0..1
/// pack four depth-cache byte references for drawing. Words 2 and 3 pack U0/V0
/// with encoded CLUT and U1/V1 with page settings; word 4 packs U2/V2 in its
/// low half and U3/V3 in its high half. UV coordinates are unsigned texel bytes.
/// References are not read here. `elemStride` counts u32 words, at least five
/// for a nonempty record; every full stride must be readable.
///
/// `preXformWrite` must provide two writable, word-aligned 52-byte POLY_GT4
/// slots per element in the selected half's first region. The first is the
/// environment layer: seeds a 4-bit additive page at VRAM word coordinates
/// (960,256) and CLUT at (256,240). Projection supplies its positions, colours
/// and normal-derived UVs later. The environment drawer replaces its page with
/// a direct-colour page, which ignores the retained CLUT. The second is the
/// opaque base: receives element texture data plus workspace `texturePageOffset`
/// (-128..127 encoded units) and `encodedClutOffset` (-8192..8128, 64 per row).
/// Address sums wrap to u16. Tags, length/code, colours, XY and SDK pads persist
/// for projection and drawing; construction performs neither phase.
///
/// With initial `elemCount` 0..65535, advances `preXformWrite` by two packets
/// per element and returns `elements + initial count * elemStride`, leaving
/// the next marker unconsumed. Consumes the count to -1 even for an empty record,
/// which reads no payload and advances neither cursor. `primWrite` is unchanged.
/// `objectFlags` is passed as zero and ignored. Capacities are unchecked. All
/// storage is borrowed for the call; no pointer is retained.
u32* tmdBuildStreamGt4PreXformEnvLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes texture fields in pre-transformed offset-layer/base triangle pairs.
///
/// `tmdBuildBufferHalf` selects this construction callback for opcode `0x4039`
/// in stage 2, areas 15 and 16. `elements` starts after the three-word header.
/// Words 0..1 contain three u16 depth-cache references for drawing and an
/// ignored high half; none are read here. Words 2 and 3 pack unsigned byte
/// U/V texel coordinates with encoded CLUT and texture-page settings. Only
/// word 4's low half supplies U2/V2; its high half is ignored.
///
/// Both packets receive the same element texture. The first is the blended
/// layer: it adds the object's `layerTexturePageOffset` (-128..127 encoded
/// units) and `layerClutRowOffset` (-128..127 palette rows, 64 units per row).
/// Page relocation wraps to u16 before ABR bit 5 is set; bit 6 is preserved,
/// yielding mode 1 or 3. The second is the opaque base: it independently adds
/// `texturePageOffset` (-128..127) and `encodedClutOffset` (-8192..8128).
/// CLUT sums also wrap to u16. Tags, commands, colours, screen positions and
/// SDK pad fields are preserved for the projection and draw passes.
///
/// The caller supplies `elemCount` (0..65535) and `elemStride` in u32 words,
/// at least five for nonempty records, with count * stride words readable.
/// `preXformWrite` must provide two writable, four-byte-aligned `POLY_GT3`
/// slots per element within the selected buffer half's first region; capacity
/// is unchecked. Workspace, live object, payload and packet storage are
/// borrowed for the call; no pointer is retained. `objectFlags` is the shared
/// callback argument, passed as zero during construction and ignored here.
///
/// Advances `preXformWrite` by two 40-byte packets per element and returns
/// `elements + initial elemCount * elemStride`, leaving the following record
/// or terminator unconsumed. Consumes the count to -1 even for an empty record,
/// which reads no payload and advances neither cursor.
u32* tmdBuildStreamGt3PreXformOffsetLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes texture fields in pre-transformed offset-layer/base quad pairs.
///
/// Construction selects this callback for `0x4079` in stage 2, areas 15 and
/// 16. `elements` starts after the three-word record header. Words 0..1 pack
/// four depth-cache byte references for drawing; none are read here. Words 2
/// and 3 pack U0/V0 with CLUT and U1/V1 with page settings; word 4 packs U2/V2
/// and U3/V3 in its low/high halves. UV coordinates are unsigned texel bytes.
/// Both packets receive that texture suffix, with independent GPU addresses.
///
/// The first packet is the blended layer: adds the live object's signed
/// `layerTexturePageOffset` (-128..127 encoded units) and `layerClutRowOffset`
/// (-128..127 palette rows, 64 encoded CLUT units per row). Page relocation
/// wraps to u16 before ABR bit 5 is set; bit 6 is preserved, yielding mode 1 or
/// 3. Drawing supplies the semitransparent command separately. The second is
/// the opaque base: independently adds workspace `texturePageOffset`
/// (-128..127) and `encodedClutOffset` (-8192..8128). CLUT sums wrap to u16.
/// Tags, length/code, colours, screen XY and SDK pads persist for later phases.
///
/// The caller supplies `elemCount` (0..65535) and `elemStride` in u32 words,
/// at least five for a nonempty record, with every full stride readable.
/// `preXformWrite` must provide two writable, word-aligned 52-byte POLY_GT4
/// slots per element in the selected half's first region. Advances that cursor
/// by two packets per element and returns `elements + initial count * elemStride`,
/// leaving the next record/marker unconsumed. Consumes the count to -1 even for
/// an empty record, which reads no payload and advances neither cursor.
/// `primWrite` is unchanged; `objectFlags` is passed as zero and ignored.
/// Capacities are unchecked. Workspace, live object, stream and packet storage
/// are borrowed for the call; no pointer is retained.
u32* tmdBuildStreamGt4PreXformOffsetLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Reserves one untextured Gouraud quad packet per element during TMD buffer construction.
///
/// The construction walk selects this handler for `0x40`, `0x60`, `0x160`,
/// `0x4040`, `0x4060` and `0x4160`. It advances `workspace->primWrite` by one
/// `POLY_G4` per element in the selected buffer half's second region, including
/// opcodes carrying `0x4000`. It reads and writes no payload or packet data;
/// positions, colours and ordering-table links are completed by the draw pass
/// for drawable records. Reserving every element keeps both passes aligned
/// even when drawing rejects a quad.
///
/// `elements` is the first payload word after the three-word record header.
/// The caller decodes `workspace->elemCount` and `workspace->elemStride` from
/// unsigned halfwords (0..65535); the stride counts u32 words. The borrowed
/// stream must contain count * stride payload words, and the word-aligned
/// primitive cursor must have room for count complete `POLY_G4` packets within
/// its region. Returns the word after the payload, leaving any following marker
/// unconsumed. The count ends at -1, including for an empty record. `objectFlags`
/// is unused; construction passes zero to satisfy the shared callback signature.
u32* modelLightingReserveStreamPrimG4(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Reserves one untextured Gouraud triangle packet per element during TMD buffer construction.
///
/// The construction walk selects this handler for `0x0`, `0x20`, `0x120`,
/// `0x4000`, `0x4020` and `0x4120`. It advances `workspace->primWrite` by one
/// `POLY_G3` per element in the selected buffer half's second region. No payload
/// or packet data is read or written: positions, colours and ordering-table
/// links are completed by the draw pass for drawable records.
///
/// `elements` is the first payload word after the three-word record header.
/// The caller decodes `workspace->elemCount` and `workspace->elemStride` from
/// unsigned halfwords (0..65535); the stride counts u32 words. The borrowed
/// stream must contain count * stride payload words, and the word-aligned
/// primitive cursor must have room for count complete `POLY_G3` packets within
/// its region. Returns the word after the payload, leaving any following marker
/// unconsumed. The count ends at -1, including for an empty record; neither
/// cursor advances for that record. `objectFlags` is the shared callback argument,
/// passed as zero during construction and ignored here.
u32* modelLightingReserveStreamPrimG3(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

#endif // GAMEPLAY_MODEL_LIGHTING_H
