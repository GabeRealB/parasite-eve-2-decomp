#include <core/pe2.h>

#include <aya/aya.h>

PE2_EDITOR_LABEL("Body")
PE2_BIND(body, model)
PE2_BIND_ARG(body, _symbol, gAyaDryfieldBodyModel)
PE2_VALUE(body)

PE2_EDITOR_LABEL("Hands")
PE2_BIND(hand_left_open, model)
PE2_BIND_ARG(hand_left_open, _symbol, gAyaDryfieldFirstPairHand0Model)
PE2_VALUE(hand_left_open)

PE2_BIND(hand_left_closed, model)
PE2_BIND_ARG(hand_left_closed, _symbol, gAyaDryfieldSecondPairHand0Model)
PE2_VALUE(hand_left_closed)

PE2_BIND(hand_right_open, model)
PE2_BIND_ARG(hand_right_open, _symbol, gAyaDryfieldFirstPairHand1Model)
PE2_VALUE(hand_right_open)

PE2_BIND(hand_right_closed, model)
PE2_BIND_ARG(hand_right_closed, _symbol, gAyaDryfieldSecondPairHand1Model)
PE2_VALUE(hand_right_closed)

PE2_EDITOR_LABEL("Face")
PE2_BIND(eyes_open, image)
PE2_BIND(eyes_half_closed, image)
PE2_BIND(eyes_closed, image)
PE2_BIND(mouth_rest, image)
PE2_BIND(mouth_alternate, image)

/* Each frame is its pixels, packed 8-bit indices four to a u_long, and an upload list of one copy and its
 * terminator, whose destination playback fills in (aya.h). */
static u_long         _gEyesOpenPixels[]  = PE2_VALUE(eyes_open);
static GpuImageUpload _gEyesOpenUploads[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, PE2_VALUE(eyes_open, _width_words), PE2_VALUE(eyes_open, _height) }, _gEyesOpenPixels },
    { GPU_IMAGE_UPLOAD_END },
};

static u_long         _gEyesHalfClosedPixels[]  = PE2_VALUE(eyes_half_closed);
static GpuImageUpload _gEyesHalfClosedUploads[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, PE2_VALUE(eyes_half_closed, _width_words), PE2_VALUE(eyes_half_closed, _height) }, _gEyesHalfClosedPixels },
    { GPU_IMAGE_UPLOAD_END },
};

static u_long         _gEyesClosedPixels[]  = PE2_VALUE(eyes_closed);
static GpuImageUpload _gEyesClosedUploads[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, PE2_VALUE(eyes_closed, _width_words), PE2_VALUE(eyes_closed, _height) }, _gEyesClosedPixels },
    { GPU_IMAGE_UPLOAD_END },
};

static u_long         _gMouthRestPixels[]  = PE2_VALUE(mouth_rest);
static GpuImageUpload _gMouthRestUploads[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, PE2_VALUE(mouth_rest, _width_words), PE2_VALUE(mouth_rest, _height) }, _gMouthRestPixels },
    { GPU_IMAGE_UPLOAD_END },
};

static u_long         _gMouthOpenPixels[]  = PE2_VALUE(mouth_alternate);
static GpuImageUpload _gMouthOpenUploads[] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 0, PE2_VALUE(mouth_alternate, _width_words), PE2_VALUE(mouth_alternate, _height) }, _gMouthOpenPixels },
    { GPU_IMAGE_UPLOAD_END },
};

GpuImageUpload* gAyaDryfieldEyesRestSequence[]    = { _gEyesOpenUploads, NULL };
GpuImageUpload* gAyaDryfieldEyesClosingSequence[] = { _gEyesOpenUploads, _gEyesHalfClosedUploads, _gEyesClosedUploads, NULL };
GpuImageUpload* gAyaDryfieldEyesOpeningSequence[] = { _gEyesClosedUploads, _gEyesHalfClosedUploads, _gEyesOpenUploads, NULL };
GpuImageUpload* gAyaDryfieldEyesBlinkSequence[]   = {
    _gEyesOpenUploads,
    _gEyesHalfClosedUploads,
    _gEyesClosedUploads,
    _gEyesHalfClosedUploads,
    _gEyesOpenUploads,
    NULL,
};
GpuImageUpload* gAyaDryfieldMouthRestSequence[] = { _gMouthRestUploads, NULL };
GpuImageUpload* gAyaDryfieldMouthOpenSequence[] = { _gMouthOpenUploads, NULL };
