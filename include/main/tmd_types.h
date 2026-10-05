#ifndef MAIN_TMD_TYPES_H
#define MAIN_TMD_TYPES_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "main/coord.h"

/// Initial local transform and parent index for one TMD model part.
///
/// `TmdSource.skeleton` contains `partCount` entries in part order. Creation
/// copies their matrices into the object's mutable coordinates and converts
/// the indices to parent links; animation changes those coordinates rather
/// than the source skeleton. Parent chains must reach a self-parented root,
/// which is initially attached to the view coordinate.
///
/// Matrix coefficients have 12 fractional bits (4096 = 1.0); translations
/// use the same integer model coordinates as the vertices.
typedef struct {
    MATRIX local;       // Initial transform in parent space
    s32    parentIndex; // Parent part index in [0, partCount); own index at a root
} TmdBone;
STATIC_ASSERT_SIZEOF(TmdBone, 0x24);

/// Shared geometry, command stream and initial hierarchy for a TMD model.
///
/// Objects borrow this record, its geometry and its stream; keep them alive
/// until all referring objects are released. Creation copies the `partCount`
/// skeleton entries into each object's mutable coordinates. Geometry remains
/// writable for morphing, and handler resolution rewrites the stream in place.
///
/// Each object's primitive buffer has two `bufferHalfBytes` halves. Within a
/// half, pre-transformed primitives occupy the first `preXformRegionBytes`
/// bytes and directly transformed primitives follow. These are capacities,
/// not stream lengths; require 0 <= preXformRegionBytes <= bufferHalfBytes <=
/// 65535 because the object caches the half size in a u16.
///
/// `partVertexCounts` describes the part-local vertex groups, but does not
/// establish the complete vertex-array extent. Neither geometry array's total
/// length is stored here. A stream-only source may omit both arrays and the
/// count table; commands must only reference geometry that is present.
///
/// Command headers contain an opcode, a draw-handler slot and a packed word
/// `(elementCount << 16) | elementStrideWords`, followed by the elements.
/// `TMD_STREAM_GROUP_END` closes a group and `TMD_STREAM_END` ends the stream.
typedef struct {
    s32            handlersResolved;    // Handler slots (0 unresolved, 1 resolved); geometry and opcodes remain writable
    s32            bufferHalfBytes;     // Byte capacity of one primitive-buffer half; both halves are allocated together
    s32            preXformRegionBytes; // Byte capacity of the first region, also the second region's offset within a half
    s32            partCount;           // Number of initial transforms and runtime coordinates; stream may have a final pre-transformed group
    const u32*     partVertexCounts;    // Part-local vertex counts: partCount entries when present, NULL for stream-only sources
    SVECTOR*       verts;               // Writable vertices in integer part-local coordinates
    SVECTOR*       normals;             // Writable normals, indexed independently of the vertices
    const TmdBone* skeleton;            // Initial pose: partCount entries, read only during object creation
    u32*           stream;              // Writable command words: opcode, handler slot, packed count/word stride, then elements
} TmdSource;
STATIC_ASSERT_SIZEOF(TmdSource, 0x24);

/// Word markers in `TmdSource.stream`.
///
/// `TMD_STREAM_GROUP_END` encodes one u32 word, 0xFFFFFFFE, with no handler
/// slot, dimensions or payload. Every group, including an empty one, ends with
/// this marker before another group or `TMD_STREAM_END`. Drawing consumes the
/// marker and advances the part slot; a final group after the skeletal parts
/// may use pre-transformed primitives without another coordinate. Buffer builds
/// and handler resolution skip the marker without changing coordinate state.
///
/// `TMD_STREAM_END` is the stream's last word. The enumerator is -1, the signed
/// immediate the walks compare, and that value is the word 0xFFFFFFFF in the
/// `u32` stream. Handler resolution, packet construction, opcode rewriting and
/// drawing stop when they read it and leave the word in place. They recognize
/// it as the stream's first word or as the word immediately after a group
/// marker, so a stream may contain only this word. It is not a command: no
/// handler slot, dimensions or payload follow it.
enum {
    TMD_STREAM_GROUP_END = -2, // Single-word command-group terminator, including groups without a skeletal part
    TMD_STREAM_END       = -1  // Last stream word (encoded 0xFFFFFFFF); walks stop and leave it unconsumed
};

/// Intrusive link for an attached model or coordinate body, also used as a
/// list's sentinel head.
///
/// `TmdObject` and `ModelObjectCoordBody` embed this as their first member. The list being
/// walked determines which container `PARENT_OF` recovers; a sentinel belongs
/// to neither container. Elements must be unlinked before their owner frees
/// them, and their link fields are not cleared by unlinking.
///
/// A sentinel's `next` points to the first element and its `prev` to the last.
/// An empty list has `next == NULL` and `prev` pointing to the sentinel itself.
/// The first element's `prev` points to the sentinel; the last's `next` is
/// `NULL`, so forward walks never visit the sentinel.
typedef struct TmdListNode {
    struct TmdListNode* next; // Next element's link; first element at a head, NULL at the end
    struct TmdListNode* prev; // Previous link; tail at a head, sentinel at the front
} TmdListNode;
STATIC_ASSERT_SIZEOF(TmdListNode, 0x8);

/// Runtime TMD model instance with its own part transforms and primitive buffer.
///
/// Creation allocates this body and `partCount` coordinates together as a
/// `TmdAllocation`; `coords` points into that tail. The source, geometry, stream,
/// light matrix and colour matrix are borrowed and must outlive their use.
/// Keep the source's nonnegative part count and buffer byte capacities unchanged
/// while the object lives; the object caches the count and half capacity.
/// Attaching the body links it on `gTmdList` and stores it in a task's
/// `extra.tmd`. Unlink it before freeing the body and its coordinate tail.
///
/// A non-NULL buffer owns two `bufferHalfBytes` byte regions in the auxiliary
/// heap. Building or drawing uses `nextBufferHalf`, then toggles it; rebuilding
/// persistent texture data in both halves takes two build passes. GPU work
/// must finish before that storage is released or reused. A NULL buffer can
/// mean deferred allocation, allocation failure, release or heap reset.
///
/// Texture relocation adds signed offsets to each command's encoded page and
/// CLUT. Layered commands have separate first-layer offsets. `shading` has the
/// interpretation selected by the resolved stream handler: a blend value with
/// 12 fractional bits, or a vertical fade distance in screen pixels. Ordinary
/// handlers need neither interpretation. Unknown bytes and flag bits have no
/// established role; they are not asserted to be padding.
typedef struct {
    TmdListNode link;                   // Attached-model list link; forward traversal ends at NULL
    GfxCoord*   coords;                 // Owned partCount-element tail; integer local translations, matrix coefficients with 12 fractional bits
    u16         flags;                  // TMD_OBJECT_* bits; unlisted bits are unproven
    s8          otOffset;               // Signed displacement in OT entries; all resulting indices must fit the selected table
    byte        unknown_F;              // Purpose unproven; creation clears this byte
    TmdSource*  source;                 // Borrowed writable geometry/stream descriptor; initial pose is copied only at creation
    u16         nextBufferHalf;         // Half selected by the next build or draw pass (0 first, 1 second); toggled after selection
    u16         bufferHalfBytes;        // Byte capacity of either half, copied from source->bufferHalfBytes (0..65535)
    void*       buffer;                 // Owned heterogeneous primitive storage: two halves, or NULL when absent
    MATRIX*     lightMtx;               // Borrowed writable light-direction matrix; draw combines it with part rotation
    MATRIX*     colorMtx;               // Borrowed writable light-colour matrix; translation holds GTE background colour
    s8          texturePageOffset;      // Signed displacement added to encoded texture-page words during a buffer build
    s8          clutRowOffset;          // Signed CLUT Y displacement; one row adds 64 to the encoded CLUT word
    s8          layerTexturePageOffset; // Signed page displacement for the first layer of offset-layer commands
    s8          layerClutRowOffset;     // Signed CLUT Y displacement for that first layer, in rows
    byte        unknown_28[0x4];        // Purpose and internal subdivision unproven; creation clears these bytes
    union {
        s32 colorBlend;                 // Layer colour/environment blend (0 reference, TMD_OBJECT_COLOR_BLEND_ONE primary)
        s32 screenFadeDistance;         // Vertical screen-space fade/displacement distance in pixels (0 disables it)
    } shading;                          // Handler-selected interpretation; not a universal lighting intensity
    s32 partCount;                      // Number of owned coordinates, copied from source->partCount; excludes stream-only final group
} TmdObject;
STATIC_ASSERT_SIZEOF(TmdObject, 0x34);

/// Established `TmdObject.flags` bits; these do not describe tmdCreateModel's flags.
enum {
    TMD_OBJECT_SEMI_TRANS      = 0x02, // Select semi-transparent forms in handlers that test the object flags
    TMD_OBJECT_FLAGGED_PASS    = 0x08, // Select the flagged draw pass independently of active-pass exclusion
    TMD_OBJECT_REVERSE_CULLING = 0x10, // Reverse facing tests in handlers that support mirrored geometry
};

/// Suppresses the sweep that allocates a primitive buffer for each attached
/// model whose buffer is NULL.
///
/// This is a bit mask in the u16 `TmdObject.flags`, not a creation flag.
/// While it is set, that sweep leaves the NULL buffer in place. Setting the
/// bit does not release a buffer that is already present, and clearing it
/// does not allocate one. Explicit allocation and release ignore the bit,
/// including the pass that fills every attached model and returns it to the
/// active draw.
///
/// Creation sets the bit when its separate buffer-flag argument has
/// `TMD_CREATE_SKIP_AUTO_BUFFER`. That argument is a different word, and its
/// bit value is 1 rather than this mask.
enum { TMD_OBJECT_SKIP_AUTO_BUFFER = 0x04 };

/// Excludes a model from the active draw pass.
///
/// This is a bit mask in the u16 `TmdObject.flags`, not a creation flag.
/// Callers also test it to suppress visible-model effects such as ground shadows.
/// Coordinate refresh and buffer allocation/release remain independent;
/// `TMD_OBJECT_FLAGGED_PASS` can still select the model for the flagged pass.
/// Creation sets this bit. Clearing it permits active drawing only when the
/// model also has a primitive buffer; it does not allocate that buffer.
enum { TMD_OBJECT_SKIP_ACTIVE_DRAW = 0x80 };

/// Full lit-colour weight for `TmdObject.shading.colorBlend`, with 12 fractional bits.
///
/// Colour-blending callers supply 0..4096. In interpolation handlers, zero
/// selects the grey reference and this value selects the lit layer colour.
/// Intermediate values weight that colour by `colorBlend` and the reference
/// by this value minus
/// `colorBlend`, with GTE products shifted right by 12. Interpolation handlers
/// bypass the blend at or above this value. Inputs below zero are unclamped.
///
/// Offset-layer handlers convert the same blend to complementary colour
/// weights with `colorBlend >> 5` and `(TMD_OBJECT_COLOR_BLEND_ONE >> 5)`
/// minus that weight. Environment-map handlers also use the blend to scale
/// the normal-derived texture-coordinate displacement. Fade steps divide
/// this integer unit by their frame count, truncating the step toward zero.
enum { TMD_OBJECT_COLOR_BLEND_ONE = 0x1000 };

/// Primary-heap model allocation containing a runtime object and its part coordinates.
///
/// The object is the first member, so the pointer returned by `tmdCreateModel`
/// also addresses the complete allocation for release. Allocate
/// `sizeof(TmdAllocation) + partCount * sizeof(GfxCoord)` bytes, where the
/// nonnegative source part count remains unchanged while the object lives.
/// `sizeof(TmdAllocation)` covers only the body; the GNU zero-length array
/// marks a variable tail with no fixed capacity.
///
/// `object.coords` points to this tail. Animation contexts borrow the same
/// array directly from the allocation; unlink the object and end those uses
/// before releasing the block. The primitive buffer is a separate allocation.
typedef struct {
    TmdObject object;    // Initial runtime body; also the allocation's release address
    GfxCoord  coords[0]; // Owned mutable transforms: object.partCount entries, in source part order
} TmdAllocation;
STATIC_ASSERT_SIZEOF(TmdAllocation, 0x34);
STATIC_ASSERT(OFFSET_OF(TmdAllocation, object) == 0, tmd_allocation_object_offset);
STATIC_ASSERT(OFFSET_OF(TmdAllocation, coords) == sizeof(TmdObject), tmd_allocation_coords_offset);

/// Borrowed scratch workspace for TMD stream construction and draw callbacks.
///
/// Construction reserves one workspace on the scratch stack. Drawing embeds
/// the same workspace at the start of a larger scratch-stack block and passes
/// its address to the callbacks. Neither pass clears it: commands may use only
/// state initialized by their pass or by an earlier command in the same walk.
/// Callbacks must not retain the workspace or its draw depth-cache pointer
/// after the walk.
///
/// Packet cursors address heterogeneous GPU packets within the selected buffer
/// half. Construction initializes texture displacements and persistent packet
/// data; drawing updates positions, colours and ordering-table links. Each
/// cursor must stay within its source-defined region. Geometry is borrowed
/// from `TmdSource`; stream geometry references encode byte offsets into
/// eight-byte SVECTOR entries, sometimes with flags in the low bits. Neither
/// geometry array's complete extent is stored in the workspace.
///
/// Drawing supplies a 1024-entry depth cache. Projection commands write GTE
/// screen Z with `TMD_VERTEX_DEPTH_INVALID` on failure; later pre-transformed
/// commands address it with four-byte entry offsets. Every depth reference
/// must fit that cache and follow the corresponding projection. OT indices,
/// including the object's signed table offset, must fit the selected table.
/// Unknown storage has no established role or subdivision.
typedef struct {
    u8*        primWrite;               // Byte cursor in the half's second region, for directly transformed packets
    u8*        preXformWrite;           // Byte cursor/base in the first region, for packets whose positions are written by projection commands
    SVECTOR*   verts;                   // Borrowed integer part-local vertices; stream references must fit the source array
    SVECTOR*   normals;                 // Borrowed part-local normals; indexed independently of vertices
    s32*       szTable;                 // Draw-only vertex depths: low 16 bits screen Z, bit 31 marks a rejected projection
    u_long*    ot;                      // Draw-only OT base, already displaced by the object's signed entry offset
    s32        elemStride;              // Element stride in u32 words, decoded from the unsigned low half of the record dimensions
    s32        elemCount;               // Initially 0..65535 elements; C handlers may decrement it through -1
    u32        opcode;                  // Complete stream-record opcode, including its handler-selection flags
    s32        gteFlag;                 // Draw-only GTE FLAG word; TMD_GTE_ERROR_FLAG is also tested with signed comparisons
    s32        gteResult;               // Draw-only reusable signed GTE result: facing area, OT depth or vertex screen Z/error marker
    u32        dispatchReturnAddress;   // Saved MIPS continuation address for the draw dispatcher
    s32        dispatchObjectFlags;     // Draw dispatcher's saved, zero-extended TmdObject.flags argument
    byte       unknown_34[0x1C];        // Purpose and internal subdivision unproven
    s16        viewLightRotation[3][3]; // Draw-only light matrix times inverse view rotation; coefficients have 12 fractional bits
    byte       unknown_62[0xE];         // Purpose and internal subdivision unproven; no matrix translation is accessed here
    s16        texturePageOffset;       // Construction-only signed encoded-page displacement (-128..127)
    s16        encodedClutOffset;       // Construction-only signed encoded CLUT displacement; one source row contributes 64 (-8192..8128)
    SVECTOR    elemNormal;              // Draw-only rotated normal scratch; some handlers scale it for texture mapping
    DVECTOR    texCoord;                // Draw-only signed screen XY, then texture-mapping intermediates; stored U/V truncate to bytes
    TmdObject* obj;                     // Borrowed runtime object whose stream and selected buffer half are being processed
    s32        otDepthShift;            // Draw-only left shift of GTE depth before selecting an OT bucket
} TmdStreamWorkspace;
STATIC_ASSERT_SIZEOF(TmdStreamWorkspace, 0x88);
STATIC_ASSERT(OFFSET_OF(TmdStreamWorkspace, dispatchReturnAddress) == 0x2C, tmd_workspace_dispatch_return_offset);
STATIC_ASSERT(OFFSET_OF(TmdStreamWorkspace, viewLightRotation) == 0x50, tmd_workspace_light_rotation_offset);
STATIC_ASSERT(OFFSET_OF(TmdStreamWorkspace, obj) == 0x80, tmd_workspace_object_offset);

/// Rejection bit in a cached TMD vertex screen-Z word.
///
/// OR into the 32-bit entry in `TmdStreamWorkspace.szTable` when the vertex
/// pre-pass rejects a projection using `TMD_GTE_ERROR_FLAG`. The low 16 bits
/// retain the GTE screen Z (0..65535); this bit makes the signed cache entry
/// negative. Pre-transformed draw handlers reject the whole primitive if any
/// corner carries it, before averaging depths for the ordering-table link.
/// Test the bit rather than equality with this value: the depth is retained.
///
/// The rejection bit records the pre-pass's decision, including a pre-pass
/// that tests a previously saved FLAG. It has the same value as
/// `TMD_GTE_ERROR_FLAG` but belongs to the depth cache, not the hardware FLAG
/// word. The unsigned literal preserves 32-bit mask operations; bit 31 does
/// not fit a signed enum constant in this compiler.
#define TMD_VERTEX_DEPTH_INVALID 0x80000000U

/// Bit 31 of the GTE FLAG word saved in `TmdStreamWorkspace.gteFlag`.
///
/// The hardware ORs FLAG bits 30..23 and 18..13 into this bit. A perspective
/// transform sets it on MAC overflow, IR1 or IR2 saturation, SZ or OTZ
/// saturation, divide overflow, or screen-coordinate saturation. IR3
/// saturation and color saturation remain their own FLAG bits. A set bit
/// means the projection failed. Direct draw skips that primitive. A vertex
/// pre-pass marks the saved screen Z with `TMD_VERTEX_DEPTH_INVALID`, and
/// later commands reject a negative depth. The two constants are the same
/// bit and name different words. Storing FLAG with `gte_stflg` publishes the
/// transform just run; a handler that does not store it tests the word a
/// previous command left there. The handwritten pre-passes test the register
/// itself with `bgez` and OR the depth marker into the cache.
///
/// `gteFlag` is signed, so a comparison with zero tests this bit as well.
/// That comparison and this mask are different instructions. Handlers that
/// hoist the mask keep the mask test. The value does not fit in a signed
/// enumerator, so it stays an unsigned macro.
#define TMD_GTE_ERROR_FLAG 0x80000000U

#endif // MAIN_TMD_TYPES_H
