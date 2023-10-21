// Copyright 2019-2020 Varjo Technologies Oy. All rights reserved.

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "DataStreamer.hpp"

#include <fstream>
#include <string>
#include <algorithm>

#include <DirectXPackedVector.h>

namespace
{
// Stream stats interval
const auto c_reportInterval = std::chrono::seconds{1};

// Number of frames to be saved
constexpr uint32_t c_numberOfSnapshotFrames = 1;

// Buffer filename prefixes
const char* c_bufferFilenames[] = {"left", "right"};

// Channel flags for channel indices
const varjo_ChannelFlag c_channelFlags[] = {varjo_ChannelFlag_First, varjo_ChannelFlag_Second};

// Convert YUV to RGB
inline void convertYUVtoRGB(int Y, int U, int V, int& R, int& G, int& B)
{
    int C = Y - 16;
    int D = U - 128;
    int E = V - 128;
    R = (298 * C + 409 * E + 128) >> 8;
    G = (298 * C - 100 * D - 208 * E + 128) >> 8;
    B = (298 * C + 516 * D + 128) >> 8;
}

// Save varjo buffer data as BMP image file
void saveBMP(const std::string& filename, const varjo_BufferMetadata& buffer, void* cpuData)
{
    LOG_DEBUG("Saving buffer to file: %s", filename.c_str());

    std::ofstream outFile(filename, std::ofstream::binary);

    if (!outFile.good()) {
        LOG_ERROR("Opening file for writing failed: %s", filename.c_str());
        return;
    }

    constexpr int32_t components = 4;

    // Write BMP headers
    BITMAPFILEHEADER bmFileHdr;
    bmFileHdr.bfType = *(reinterpret_cast<const WORD*>("BM"));
    bmFileHdr.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + (components * buffer.width * buffer.height);
    bmFileHdr.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    outFile.write(reinterpret_cast<const char*>(&bmFileHdr), sizeof(bmFileHdr));
    if (!outFile.good()) {
        LOG_ERROR("Writing to bitmap file failed: %s", filename.c_str());
        return;
    }

    BITMAPINFOHEADER bmInfoHdr;
    bmInfoHdr.biSize = sizeof(BITMAPINFOHEADER);
    bmInfoHdr.biWidth = buffer.width;
    bmInfoHdr.biHeight = buffer.height;
    bmInfoHdr.biPlanes = 1;
    bmInfoHdr.biBitCount = 32;
    bmInfoHdr.biCompression = BI_RGB;
    bmInfoHdr.biSizeImage = 0;
    bmInfoHdr.biXPelsPerMeter = bmInfoHdr.biYPelsPerMeter = 2835;
    bmInfoHdr.biClrImportant = bmInfoHdr.biClrUsed = 0;
    outFile.write(reinterpret_cast<const char*>(&bmInfoHdr), sizeof(bmInfoHdr));
    if (!outFile.good()) {
        LOG_ERROR("Writing to bitmap file failed: %s", filename.c_str());
        return;
    }

    switch (buffer.format) {
        case varjo_TextureFormat_RGBA16_FLOAT: {
            // Background color for alpha blending
            const float rgbBackground[3] = {0.25f, 0.45f, 0.40f};

            // Convert half float to uint8
            const DirectX::PackedVector::HALF* halfSrc = reinterpret_cast<const DirectX::PackedVector::HALF*>(cpuData);
            halfSrc += buffer.byteSize / sizeof(DirectX::PackedVector::HALF);
            for (int32_t y = 0; y < buffer.height; y++) {
                halfSrc -= buffer.rowStride / sizeof(DirectX::PackedVector::HALF);
                std::vector<uint8_t> line(buffer.width * components);
                for (int32_t x = 0; x < buffer.width * components; x += components) {
                    // Streamed RGB values are in linear colorspace so we gamma correct them for screen here
                    constexpr float gamma = 1.0f / 2.2f;

                    // Read alpha
                    const float alpha = DirectX::PackedVector::XMConvertHalfToFloat(halfSrc[x + 3]);

                    // Read value, gamma correct, alpha blend to background color, write to output in BMP byte order
                    for (int32_t c = 0; c < 3; c++) {
                        float value = powf(DirectX::PackedVector::XMConvertHalfToFloat(halfSrc[x + c]), gamma);
                        value = value * alpha + rgbBackground[c] * (1.0f - alpha);
                        line[x + (2 - c)] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, 255.0f * value)));
                    }

                    // Write alpha
                    line[x + 3] = 255;
                    // line[x + 3] = static_cast<uint8_t>(max(0, min(255, 255.0 * alpha)));
                }

                // Write line to file
                outFile.write(reinterpret_cast<const char*>(line.data()), line.size());
                if (!outFile.good()) {
                    LOG_ERROR("Writing to bitmap file failed: %s", filename.c_str());
                    return;
                }
            }
        } break;

        case varjo_TextureFormat_YUV422: {
            // Convert YUV422 to RGBA8
            const uint8_t* b = reinterpret_cast<const uint8_t*>(cpuData);
            b += (buffer.rowStride * buffer.height);
            const uint32_t uvOffs = (buffer.rowStride * buffer.height);
            for (int32_t y = 0; y < buffer.height; y++) {
                std::vector<uint8_t> line(buffer.width * components);
                b -= buffer.rowStride;
                size_t lineOffs = 0;
                for (int32_t x = 0; x < buffer.width; x++) {
                    int Y = b[x];
                    auto uvX = x - (x & 1);
                    int U = b[uvX + 0 + uvOffs];
                    int V = b[uvX + 1 + uvOffs];

                    int R, G, B;
                    convertYUVtoRGB(Y, U, V, R, G, B);

                    // Write RGBA in BMP byteorder
                    line[lineOffs + 2] = std::max(std::min(R, 255), 0);
                    line[lineOffs + 1] = std::max(std::min(G, 255), 0);
                    line[lineOffs + 0] = std::max(std::min(B, 255), 0);
                    line[lineOffs + 3] = 255;
                    lineOffs += components;
                }
                outFile.write(reinterpret_cast<const char*>(line.data()), line.size());
                if (!outFile.good()) {
                    LOG_ERROR("Writing to bitmap file failed: %s", filename.c_str());
                    return;
                }
            }
        } break;

        case varjo_TextureFormat_NV12: {
            // Convert YUV420 NV12 to RGBA8
            const uint8_t* bY = reinterpret_cast<const uint8_t*>(cpuData);
            bY += (buffer.rowStride * buffer.height);
            const uint8_t* bUV = bY + (buffer.rowStride * (buffer.height >> 1));

            for (int32_t y = 0; y < buffer.height; y++) {
                std::vector<uint8_t> line(buffer.width * components);
                bY -= buffer.rowStride;
                if ((y & 1) == 0) {
                    bUV -= buffer.rowStride;
                }
                size_t lineOffs = 0;
                for (int32_t x = 0; x < buffer.width; x++) {
                    int Y = bY[x];

                    auto uvX = x - (x & 1);
                    int U = bUV[uvX + 0];
                    int V = bUV[uvX + 1];

                    int R, G, B;
                    convertYUVtoRGB(Y, U, V, R, G, B);

                    // Write RGBA in BMP byteorder
                    line[lineOffs + 2] = std::max(std::min(R, 255), 0);
                    line[lineOffs + 1] = std::max(std::min(G, 255), 0);
                    line[lineOffs + 0] = std::max(std::min(B, 255), 0);
                    line[lineOffs + 3] = 255;
                    lineOffs += components;
                }
                outFile.write(reinterpret_cast<const char*>(line.data()), line.size());
                if (!outFile.good()) {
                    LOG_ERROR("Writing to bitmap file failed: %s", filename.c_str());
                    return;
                }
            }
        } break;

        default: {
            CRITICAL("Unsupported pixel format: %d", static_cast<int>(buffer.format));
        } break;
    }

    outFile.close();
    LOG_INFO("File saved succesfully: %s", filename.c_str());
}

}  // namespace

namespace VarjoExamples
{
DataStreamer::DataStreamer(varjo_Session* session)
    : m_session(session)
    , m_hmdPose(toVarjoMatrix(glm::mat4(0.0f)))
{
}

DataStreamer::~DataStreamer()
{
    // Notice that this destructor waits for stream lock. It might be better to have separate function
    // for cleaning up possibly running data streams to prevent destructor blocking

    // Lock streaming data to prevent callbacks
    std::lock_guard<std::recursive_mutex> streamLock(m_streamData.mutex);

    // If we have streams running, stop them
    for (auto streamId : m_streamData.streamIds) {
        LOG_WARNING("Stopping running data stream: %d", static_cast<int>(streamId));
        varjo_StopDataStream(m_session, streamId);
    }

    // Reset session
    m_session = nullptr;
}

std::pair<varjo_StreamId, varjo_ChannelFlag> DataStreamer::getStreamingIdAndChannel(varjo_StreamType streamType, varjo_TextureFormat streamFormat) const
{
    std::pair<varjo_StreamId, varjo_ChannelFlag> streamInfo = std::make_pair(varjo_InvalidId, varjo_ChannelFlag_None);
    {
        // Frame callback comes from different thread, lock streaming data
        std::lock_guard<std::recursive_mutex> streamLock(m_streamData.mutex);

        // Find out if we have running stream
        auto it = m_streamData.streamMapping.find({streamType, streamFormat});
        if (it != m_streamData.streamMapping.end()) {
            streamInfo = it->second;
        }
    }
    return streamInfo;
}

bool DataStreamer::isStreaming() const
{
    // Frame callback comes from different thread, lock streaming data
    std::lock_guard<std::recursive_mutex> streamLock(m_streamData.mutex);

    // Find out if we have running streams
    return !m_streamData.streamMapping.empty();
}

bool DataStreamer::isStreaming(varjo_StreamType streamType, varjo_TextureFormat streamFormat) const
{
    return (getStreamingIdAndChannel(streamType, streamFormat).first != varjo_InvalidId);
}

varjo_TextureFormat DataStreamer::getFormat(varjo_StreamType streamType)
{
    std::vector<varjo_StreamConfig> configs;
    configs.resize(varjo_GetDataStreamConfigCount(m_session));
    varjo_GetDataStreamConfigs(m_session, configs.data(), static_cast<int32_t>(configs.size()));
    CHECK_VARJO_ERR(m_session);

    for (const auto& config : configs) {
        if (config.streamType == streamType) {
            return config.format;
        }
    }

    return varjo_TextureFormat_INVALID;
}

bool DataStreamer::isStreaming(varjo_StreamType streamType, varjo_TextureFormat streamFormat, varjo_ChannelFlag& outChannels) const
{
    auto streamingInfo = getStreamingIdAndChannel(streamType, streamFormat);
    outChannels = streamingInfo.second;
    return (streamingInfo.first != varjo_InvalidId);
}

void DataStreamer::startDataStream(varjo_StreamType streamType, varjo_TextureFormat streamFormat, varjo_ChannelFlag channels)
{
    varjo_StreamId streamId = getStreamingIdAndChannel(streamType, streamFormat).first;

    if (streamId == varjo_InvalidId) {
        LOG_INFO("Start streaming: type=%lld, format=%lld", streamType, streamFormat);

        if (!isStreaming()) {
            m_statusLine = "Starting stream.";
        }

        // Start stream
        streamId = startStreaming(streamType, streamFormat, channels);

        // Check if successfully started
        if (streamId != varjo_InvalidId) {
            // Frame callback comes from different thread, lock streaming data
            std::lock_guard<std::recursive_mutex> streamLock(m_streamData.mutex);

            m_streamData.frameCounts[{streamId, varjo_ChannelIndex_First}] = 0;
            m_streamData.frameCounts[{streamId, varjo_ChannelIndex_Second}] = 0;

            m_streamData.streamIds.emplace(streamId);
            m_streamData.streamMapping[{streamType, streamFormat}] = std::make_pair(streamId, channels);

            // Reset stats if first stream
            if (m_streamData.streamIds.size() == 1) {
                m_stats = {};
                m_stats.reportTime = std::chrono::high_resolution_clock::now();
            }
        }
    } else {
        LOG_WARNING("Start stream failed. Already running: type=%lld, format=%lld", streamType, streamFormat);
    }
}

void DataStreamer::stopDataStream(varjo_StreamType streamType, varjo_TextureFormat streamFormat)
{
    varjo_StreamId streamId = getStreamingIdAndChannel(streamType, streamFormat).first;

    if (streamId != varjo_InvalidId) {
        LOG_INFO("Stop streaming: type=%lld", streamType);

        // Stop stream
        varjo_StopDataStream(m_session, streamId);
        CHECK_VARJO_ERR(m_session);

        // Scope lock for cleanup
        {
            // Frame callback comes from different thread, lock streaming data
            std::lock_guard<std::recursive_mutex> streamLock(m_streamData.mutex);

            m_streamData.frameCounts.erase({streamId, varjo_ChannelIndex_First});
            m_streamData.frameCounts.erase({streamId, varjo_ChannelIndex_Second});
            m_streamData.streamIds.erase(streamId);
            m_streamData.streamMapping.erase({streamType, streamFormat});

            // Reset frame exposure and white balance
            if (streamType == varjo_StreamType_DistortedColor) {
                m_frameExposure = {};
            }
        }

        if (!isStreaming()) {
            m_statusLine = "";
        }

    } else {
        LOG_WARNING("Stop stream failed. Not running: type=%lld, format=%lld", streamType, streamFormat);
    }
}

void DataStreamer::handleDelayedBuffers(bool ignore)
{
    // Callback comes from different thread, lock streaming data
    std::lock_guard<std::recursive_mutex> streamLock(m_streamData.mutex);

    if (m_streamData.delayedBuffers.empty()) {
        return;
    }

    // Handle buffers if not ignored
    if (!ignore) {
        LOG_DEBUG("Handling delayed stream buffers: count=%d", m_streamData.delayedBuffers.size());
        for (auto& db : m_streamData.delayedBuffers) {
            storeBuffer(db.type, db.streamId, db.channelIndex, db.frameNumber, db.bufferId, db.buffer, db.cpuBuffer, db.baseName);
        }
    } else {
        LOG_DEBUG("Ignoring delayed stream buffers: count=%d", m_streamData.delayedBuffers.size());

        for (auto& db : m_streamData.delayedBuffers) {
            // Just unlock buffer to allow reuse
            LOG_DEBUG("Unlocking buffer (id=%lld)", db.bufferId);
            varjo_UnlockDataStreamBuffer(m_session, db.bufferId);
            CHECK_VARJO_ERR(m_session);
        }
    }

    // Clear buffers
    m_streamData.delayedBuffers.clear();
}

void DataStreamer::printStreamConfigs() const
{
    std::vector<varjo_StreamConfig> configs;
    configs.resize(varjo_GetDataStreamConfigCount(m_session));
    varjo_GetDataStreamConfigs(m_session, configs.data(), static_cast<int32_t>(configs.size()));
    CHECK_VARJO_ERR(m_session);

    LOG_INFO("\nStream configs:");
    for (const auto& config : configs) {
        LOG_INFO("  Stream: id=%lld, type=%lld, bufferType=%lld, format=%lld, channels=%lld, fps=%d, w=%d, h=%d, stride=%d", config.streamId, config.streamType,
            config.bufferType, config.format, config.channelFlags, config.frameRate, config.width, config.height, config.rowStride);
    }
    LOG_INFO("");
}

void DataStreamer::storeBuffer(varjo_StreamType type, varjo_StreamId streamId, varjo_ChannelIndex channelIdx, int64_t frameNumber, varjo_BufferId bufferId,
    varjo_BufferMetadata& buffer, void* cpuData, const std::string& baseName)
{
    // Check that stream hasnot been stopped and removed already
    if (m_streamData.frameCounts.count({streamId, channelIdx}) == 0) {
        return;
    }

    // Handle buffer
    if (buffer.type == varjo_BufferType_CPU) {
        assert(cpuData);
        assert(buffer.format == varjo_TextureFormat_RGBA16_FLOAT || buffer.format == varjo_TextureFormat_YUV422 || buffer.format == varjo_TextureFormat_NV12);

        auto& frameCount = m_streamData.frameCounts.find({streamId, channelIdx})->second;
        //if (frameCount < c_numberOfSnapshotFrames) {
            // Save buffer data to file
            //std::string fileName =
                //baseName + "_sid" + std::to_string(streamId) + "_frm" + std::to_string(frameNumber) + "_bid" + std::to_string(bufferId) + ".bmp";

		if (frameCount % 60 == 0) {
			std::string fileName = "frames/" + baseName + ".bmp";
			saveBMP(fileName, buffer, cpuData);
		}
        //}

        // Store latest cubemap frame.
        if (type == varjo_StreamType_EnvironmentCubemap) {
            // Resize buffer if needed.
            if (m_latestCubemapFrame.data.size() != buffer.byteSize) {
                m_latestCubemapFrame.data.resize(buffer.byteSize);
            }

            memcpy(m_latestCubemapFrame.data.data(), cpuData, buffer.byteSize);
            m_latestCubemapFrame.metadata = buffer;
        }

        frameCount++;

    } else if (buffer.type == varjo_BufferType_GPU) {
        assert(cpuData == nullptr);
        CRITICAL("GPU buffers not currently supported!");
    } else {
        CRITICAL("Unsupported output type!");
    }

    // Unlock buffer
    LOG_DEBUG("Unlocking buffer (id=%lld)", bufferId);
    varjo_UnlockDataStreamBuffer(m_session, bufferId);
    CHECK_VARJO_ERR(m_session);
}

void DataStreamer::handleBuffer(
    varjo_StreamType type, varjo_StreamId streamId, varjo_ChannelIndex channelIdx, int64_t frameNumber, varjo_BufferId bufferId, const std::string& baseName)
{
    // Lock buffer
    varjo_LockDataStreamBuffer(m_session, bufferId);
    CHECK_VARJO_ERR(m_session);

    varjo_BufferMetadata meta = varjo_GetBufferMetadata(m_session, bufferId);
    void* cpuData = varjo_GetBufferCPUData(m_session, bufferId);

    LOG_DEBUG("Locked buffer (id=%lld): res=%dx%d, stride=%u, bytes=%u, type=%d, format=%d", bufferId, meta.width, meta.height, meta.rowStride, meta.byteSize,
        (int)meta.type, (int)meta.format);

    bool delayed = m_delayedBufferHandling;

    if (delayed) {
        DelayedBuffer delayedBuffer;
        delayedBuffer.type = type;
        delayedBuffer.streamId = streamId;
        delayedBuffer.channelIndex = channelIdx;
        delayedBuffer.frameNumber = frameNumber;
        delayedBuffer.bufferId = bufferId;
        delayedBuffer.baseName = baseName;
        delayedBuffer.buffer = meta;
        delayedBuffer.cpuBuffer = cpuData;

        // Add to delayed buffers. Will be handled in main loop.
        m_streamData.delayedBuffers.emplace_back(delayedBuffer);

    } else {
        // Handle buffer immediately
        storeBuffer(type, streamId, channelIdx, frameNumber, bufferId, meta, cpuData, baseName);
    }
}

void DataStreamer::dataStreamFrameCallback(const varjo_StreamFrame* frame, varjo_Session* session, void* userData)
{
    // This callback is called by Varjo runtime from a separate stream specific thread.
    // To avoid dropping frames, the callback should be as lightweight as possible.
    // i.e. file writing should be offloaded to a different thread.

    DataStreamer* streamer = reinterpret_cast<DataStreamer*>(userData);
    streamer->onDataStreamFrame(frame, session);
}

void DataStreamer::onDataStreamFrame(const varjo_StreamFrame* frame, varjo_Session* session)
{
    // Callback comes from different thread, lock streaming data
    std::lock_guard<std::recursive_mutex> streamLock(m_streamData.mutex);

    m_stats.frameCount++;
    const auto now = std::chrono::high_resolution_clock::now();
    const auto delta = now - m_stats.reportTime;
    if (delta >= c_reportInterval) {
        m_statusLine = "Got " + std::to_string(m_stats.frameCount) + " frames from " + std::to_string(m_streamData.streamIds.size()) + " streams in last " +
                       std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(delta).count()) + " ms";
        m_stats = {};
        m_stats.reportTime = now;
    }

    // Check that client session hasn't already be reset in destructor. Should never happen!
    if (session != m_session) {
        LOG_ERROR("Invalid session in callback.");
        return;
    }

    // Check that the stream is still running
    if (m_streamData.streamIds.count(frame->id) == 0) {
        LOG_WARNING("Frame callback ignored. Stream already deleted: type=%lld, id=%lld", frame->type, frame->id);
        return;
    }

    switch (frame->type) {
        case varjo_StreamType_DistortedColor: {
            LOG_DEBUG("Got frame #%lld: id=%lld, type=%lld, time=%.3f, exposure=%.2f, ev=%.2f, temp=%.2f, rgb=(%.2f, %.2f, %.2f)", frame->frameNumber,
                frame->id, frame->type, 1E-9 * frame->metadata.distortedColor.timestamp, frame->metadata.distortedColor.exposureTime,
                frame->metadata.distortedColor.ev, frame->metadata.distortedColor.whiteBalanceTemperature,
                frame->metadata.distortedColor.wbNormalizationData.whiteBalanceColorGains[0],
                frame->metadata.distortedColor.wbNormalizationData.whiteBalanceColorGains[1],
                frame->metadata.distortedColor.wbNormalizationData.whiteBalanceColorGains[2]);

            // Store frame exposure data
            m_frameExposure.exposureTime = frame->metadata.distortedColor.exposureTime;
            m_frameExposure.ev = frame->metadata.distortedColor.ev;
            m_frameExposure.cameraCalibrationConstant = frame->metadata.distortedColor.cameraCalibrationConstant;
            m_frameExposure.wbNormalizationData = frame->metadata.distortedColor.wbNormalizationData;
            m_frameExposure.valid = true;

            // Store HMD pose
            m_hmdPose = frame->hmdPose;

            std::vector<varjo_ChannelIndex> channelIndices;
            if (frame->channels & varjo_ChannelFlag_Left) {
                channelIndices.push_back(varjo_ChannelIndex_Left);
            }

            if (frame->channels & varjo_ChannelFlag_Right) {
                channelIndices.push_back(varjo_ChannelIndex_Right);
            }

            for (const varjo_ChannelIndex& channelIndex : channelIndices) {
                LOG_DEBUG("  Channel index: #%lld", channelIndex);

                if (frame->dataFlags & varjo_DataFlag_Extrinsics) {
                    varjo_Matrix extrinsics = varjo_GetCameraExtrinsics(session, frame->id, frame->frameNumber, channelIndex);
                    CHECK_VARJO_ERR(m_session);
                }

                if (frame->dataFlags & varjo_DataFlag_Intrinsics) {
                    varjo_CameraIntrinsics intrinsics = varjo_GetCameraIntrinsics(session, frame->id, frame->frameNumber, channelIndex);
                    CHECK_VARJO_ERR(m_session);
                }

                varjo_BufferId bufferId = varjo_InvalidId;
                if (frame->dataFlags & varjo_DataFlag_Buffer) {
                    bufferId = varjo_GetBufferId(session, frame->id, frame->frameNumber, channelIndex);
                    CHECK_VARJO_ERR(m_session);
                }

                if (bufferId == varjo_InvalidId) {
                    LOG_WARNING("    (no buffer)");
                    return;
                }

                // Check which channels were requested
                varjo_ChannelFlag requestedChannelFlags = varjo_ChannelFlag_None;
                for (auto& s : m_streamData.streamMapping) {
                    if (s.second.first == frame->id) {
                        requestedChannelFlags = s.second.second;
                    }
                }

                // Only handle buffer if the channel was requested
                if (requestedChannelFlags & c_channelFlags[channelIndex]) {
                    handleBuffer(frame->type, frame->id, channelIndex, frame->frameNumber, bufferId, std::string(c_bufferFilenames[channelIndex]));
                }
            }
        } break;

        case varjo_StreamType_EnvironmentCubemap: {
            LOG_DEBUG("Got frame #%lld: id=%lld, type=%lld, time=%.3f", frame->frameNumber, frame->id, frame->type,
                1E-9 * frame->metadata.environmentCubemap.timestamp);

            if (!(frame->channels & varjo_ChannelFlag_First)) {
                LOG_WARNING("    (missing first buffer)");
                return;
            }

            varjo_BufferId bufferId = varjo_GetBufferId(session, frame->id, frame->frameNumber, varjo_ChannelIndex_First);

            if (bufferId == varjo_InvalidId) {
                LOG_WARNING("    (no buffer)");
                return;
            }

            handleBuffer(frame->type, frame->id, varjo_ChannelIndex_First, frame->frameNumber, bufferId, "cube");

        } break;

        default: {
            CRITICAL("Unsupported stream type: %lld", frame->type);
        } break;
    }
}

varjo_StreamId DataStreamer::startStreaming(varjo_StreamType type, varjo_TextureFormat format, varjo_ChannelFlag channels)
{
    varjo_StreamId streamId = varjo_InvalidId;

    // Fetch stream configs
    std::vector<varjo_StreamConfig> configs;
    configs.resize(varjo_GetDataStreamConfigCount(m_session));
    varjo_GetDataStreamConfigs(m_session, configs.data(), static_cast<int32_t>(configs.size()));
    CHECK_VARJO_ERR(m_session);

    // Find suitable stream, start the frame stream, and provide callback for handling frames.
    for (const auto& conf : configs) {
        if (conf.streamType == type) {
            if ((type == varjo_StreamType_DistortedColor) && (conf.bufferType == varjo_BufferType_CPU) && (conf.channelFlags & varjo_ChannelFlag_Left) &&
                (conf.channelFlags & varjo_ChannelFlag_Right) && (conf.format == format)) {
                varjo_StartDataStream(m_session, conf.streamId, channels, dataStreamFrameCallback, this);
                if (CHECK_VARJO_ERR(m_session) == varjo_NoError) {
                    streamId = conf.streamId;
                }
                break;
            } else if ((type == varjo_StreamType_EnvironmentCubemap) && (conf.bufferType == varjo_BufferType_CPU) &&
                       (conf.channelFlags & varjo_ChannelFlag_First) && (conf.format == format)) {
                varjo_StartDataStream(m_session, conf.streamId, varjo_ChannelFlag_First, dataStreamFrameCallback, this);
                if (CHECK_VARJO_ERR(m_session) == varjo_NoError) {
                    streamId = conf.streamId;
                }
                break;
            }
        }
    }

    return streamId;
}

bool DataStreamer::isDelayedBufferHandlingEnabled() const { return m_delayedBufferHandling; }

void DataStreamer::setDelayedBufferHandlingEnabled(bool enabled) { m_delayedBufferHandling = enabled; }

DataStreamer::ExposureAdjustments DataStreamer::getExposureAdjustments() const
{
    std::lock_guard<std::recursive_mutex> streamLock(m_streamData.mutex);
    return m_frameExposure;
}

varjo_Matrix DataStreamer::getHmdPose() const
{
    std::lock_guard<std::recursive_mutex> streamLock(m_streamData.mutex);
    return m_hmdPose;
}

bool DataStreamer::getCubemapFrame(CubemapFrame& frame) const
{
    std::lock_guard<std::recursive_mutex> streamLock(m_streamData.mutex);

    // Check if we have a cubemap frame available.
    if (m_latestCubemapFrame.data.empty()) {
        return false;
    }

    // Create a copy of the cubemap data so it can be used safely without worrying about synchronization.
    frame = m_latestCubemapFrame;
    return true;
}

}  // namespace VarjoExamples
