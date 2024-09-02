#include "codec/h264/h264_encoder.h"

#include "vector"

#include "common/logging.h"
#include "common/utils.h"

std::unique_ptr<H264Encoder> H264Encoder::Create(Args args) {
    auto ptr = std::make_unique<H264Encoder>(args);
    ptr->Init();
    return ptr;
}

H264Encoder::H264Encoder(Args args)
    : fps_(args.fps),
      width_(args.width),
      height_(args.height),
      bitrate_(2000000),
      encoder_(nullptr) {}

H264Encoder::~H264Encoder() { ReleaseCodec(); }

void H264Encoder::Init() {
    int rv = WelsCreateSVCEncoder(&encoder_);
    if (rv != 0) {
        std::cerr << "Failed to create OpenH264 encoder." << std::endl;
        return;
    }

    SEncParamExt encoder_param;
    encoder_->GetDefaultParams(&encoder_param);
    encoder_param.iUsageType = CAMERA_VIDEO_REAL_TIME;
    encoder_param.iTemporalLayerNum = 0;
    encoder_param.uiIntraPeriod = 15;
    encoder_param.iRCMode = RC_BITRATE_MODE;
    encoder_param.bEnableFrameSkip = true;
    encoder_param.iSpatialLayerNum = 1;
    SSpatialLayerConfig *spartialLayerConfiguration = &encoder_param.sSpatialLayers[0];
    spartialLayerConfiguration->uiProfileIdc = PRO_HIGH;
    encoder_param.iPicWidth = spartialLayerConfiguration->iVideoWidth = width_;
    encoder_param.iPicHeight = spartialLayerConfiguration->iVideoHeight = height_;
    encoder_param.fMaxFrameRate = spartialLayerConfiguration->fFrameRate = fps_;
    encoder_param.iTargetBitrate = spartialLayerConfiguration->iSpatialBitrate = bitrate_;
    encoder_param.iMaxBitrate = spartialLayerConfiguration->iMaxSpatialBitrate = bitrate_ * 1.5;

    rv = encoder_->InitializeExt(&encoder_param);
    if (rv != 0) {
        std::cerr << "Failed to initialize OpenH264 encoder." << std::endl;
        return;
    }
}

void H264Encoder::Encode(rtc::scoped_refptr<webrtc::I420BufferInterface> frame_buffer,
                         std::function<void(uint8_t *, int)> on_capture) {
    src_pic_ = {0};
    src_pic_.iPicWidth = width_;
    src_pic_.iPicHeight = height_;
    src_pic_.iColorFormat = videoFormatI420;
    src_pic_.iStride[0] = frame_buffer->StrideY();;
    src_pic_.iStride[1] = frame_buffer->StrideU();
    src_pic_.iStride[2] = frame_buffer->StrideV();
    src_pic_.pData[0] = const_cast<uint8_t*>(frame_buffer->DataY());
    src_pic_.pData[1] = const_cast<uint8_t*>(frame_buffer->DataU());
    src_pic_.pData[2] = const_cast<uint8_t*>(frame_buffer->DataV());

    SFrameBSInfo info;
    memset(&info, 0, sizeof(SFrameBSInfo));
    int rv = encoder_->EncodeFrame(&src_pic_, &info);

    if (info.eFrameType != videoFrameTypeSkip) {
        int required_capacity = 0;
        for (int i = 0; i < info.iLayerNum; i++) {
            const SLayerBSInfo *layer = &info.sLayerInfo[i];
            for (int nal = 0; nal < layer->iNalCount; ++nal) {
                required_capacity += layer->pNalLengthInByte[nal];
            }
        }

        std::vector<uint8_t> encoded_buf(required_capacity);

        int encoded_size = 0;
        for (int i = 0; i < info.iLayerNum; i++) {
            const SLayerBSInfo *layer = &info.sLayerInfo[i];
            int layer_len = 0;
            for (int nal = 0; nal < layer->iNalCount; ++nal) {
                layer_len += layer->pNalLengthInByte[nal];
            }

            memcpy(encoded_buf.data() + encoded_size, layer->pBsBuf, layer_len);
            encoded_size += layer_len;
        }

        on_capture(encoded_buf.data(), encoded_size);
    }
}

void H264Encoder::ReleaseCodec() {
    encoder_->Uninitialize();
    WelsDestroySVCEncoder(encoder_);

    printf("sw h264 encode was released!\n");
}