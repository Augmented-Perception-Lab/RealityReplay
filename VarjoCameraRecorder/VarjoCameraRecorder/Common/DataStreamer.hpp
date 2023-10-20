// Copyright 2019-2020 Varjo Technologies Oy. All rights reserved.

#pragma once

#include <mutex>
#include <vector>
#include <map>
#include <unordered_set>
#include <atomic>
#include <array>
#include <chrono>

#include <Varjo_datastream.h>

#include "Globals.hpp"

namespace VarjoExamples
{
//! Simple example class for testing Varjo data streaming
class DataStreamer
{
public:
    //! VST camera exposure/color adjustments for matching VR scene to VST
    struct ExposureAdjustments {
        bool valid = false;                             //!< Are values valid
        double ev = 0.0;                                //!< Exposure EV at ISO100
        double cameraCalibrationConstant = 0.0;         //!< Camera calibration constant
        varjo_WBNormalizationData wbNormalizationData;  //!< White balance normalization data.
        double exposureTime = 0.0;                      //!< Exposure time in seconds
    };

    //! HDR cubemap frame data.
    struct CubemapFrame {
        varjo_BufferMetadata metadata;  //!< Cubemap frame metadata
        std::vector<uint8_t> data;      //!< Cubemap frame data
    };

    //! Construct data streamer
    DataStreamer(varjo_Session* session);

    //! Destruct data streamer. Cleans up running data streams.
    ~DataStreamer();

    // Disable copy, move and assign
    DataStreamer(const DataStreamer& other) = delete;
    DataStreamer(const DataStreamer&& other) = delete;
    DataStreamer& operator=(const DataStreamer& other) = delete;
    DataStreamer& operator=(const DataStreamer&& other) = delete;

    // Returns preferred texture format for given type
    varjo_TextureFormat getFormat(varjo_StreamType streamType);

    //! Start data streaming
    void startDataStream(varjo_StreamType streamType, varjo_TextureFormat streamFormat, varjo_ChannelFlag channels);

    //! Stop data streaming
    void stopDataStream(varjo_StreamType streamType, varjo_TextureFormat streamFormat);

    //! Is streaming
    bool isStreaming() const;

    //! Is streaming given type and format
    bool isStreaming(varjo_StreamType streamType, varjo_TextureFormat streamFormat) const;

    //! Return if streaming and if so, get streaming channels
    bool isStreaming(varjo_StreamType streamType, varjo_TextureFormat streamFormat, varjo_ChannelFlag& outChannels) const;

    //! Handle delayed data stream buffers. Optionally ignore frames
    void handleDelayedBuffers(bool ignore = false);

    //! Print out currently available data stream configs
    void printStreamConfigs() const;

    //! Is delayed bufferhandling currently enabled
    bool isDelayedBufferHandlingEnabled() const;

    //! Set delayed bufferhandling enabled
    void setDelayedBufferHandlingEnabled(bool enabled);

    //! Get latest exposure/color adjustments for matching VR scene to camera parameters
    ExposureAdjustments getExposureAdjustments() const;

    //! Get latest HMD pose
    varjo_Matrix getHmdPose() const;

    //! Get latest cube map frame
    bool getCubemapFrame(CubemapFrame& frame) const;

    //! Return status line
    std::string getStatusLine() const { return isStreaming() ? (m_statusLine.empty() ? "Not streaming." : m_statusLine) : "Not streaming."; }

private:
    //! Static data stream frame callback function
    static void dataStreamFrameCallback(const varjo_StreamFrame* frame, varjo_Session* session, void* userData);

    //! Called from data stream frame callback
    void onDataStreamFrame(const varjo_StreamFrame* frame, varjo_Session* session);

    //! Handle frame buffer
    void handleBuffer(varjo_StreamType type, varjo_StreamId streamId, varjo_ChannelIndex channelIdx, int64_t frameNumber, varjo_BufferId bufferId,
        const std::string& baseName);

    //! Store buffer contents to file
    void storeBuffer(varjo_StreamType type, varjo_StreamId streamId, varjo_ChannelIndex channelIdx, int64_t frameNumber, varjo_BufferId bufferId,
        varjo_BufferMetadata& buffer, void* cpuData, const std::string& baseName);

    //! Find data stream of given type and texture format and start it
    varjo_StreamId startStreaming(varjo_StreamType streamType, varjo_TextureFormat streamFormat, varjo_ChannelFlag channels);

    //! Get streaming ID
    std::pair<varjo_StreamId, varjo_ChannelFlag> getStreamingIdAndChannel(varjo_StreamType streamType, varjo_TextureFormat streamFormat) const;

private:
    //! Delayed buffer info structure
    struct DelayedBuffer {
        varjo_StreamType type;                                       //!< Stream type for this buffer
        varjo_StreamId streamId = varjo_InvalidId;                   //!< Stream Id for this buffer
        varjo_ChannelIndex channelIndex = varjo_ChannelIndex_First;  //!< Channel index
        int64_t frameNumber = 0;                                     //!< Buffer frame number
        std::string baseName;                                        //!< Base filename
        varjo_BufferId bufferId = varjo_InvalidId;                   //!< Varjo buffer identifier
        varjo_BufferMetadata buffer;                                 //!< Varjo buffer metadata
        void* cpuBuffer;                                             //!< Pointer to CPU buffer data
    };

    //! Struct for thread safe stream data
    struct StreamData {
        mutable std::recursive_mutex mutex;            //!< Mutex for locking streamer data
        std::unordered_set<varjo_StreamId> streamIds;  //!< Set of running streams
        std::map<std::pair<varjo_StreamType, varjo_TextureFormat>, std::pair<varjo_StreamId, varjo_ChannelFlag>>
            streamMapping;                                                             //!< Stream id+channels for each stream type+format pair
        std::map<std::pair<varjo_StreamId, varjo_ChannelIndex>, int64_t> frameCounts;  //!< Frame counters for stream IDs
        std::vector<DelayedBuffer> delayedBuffers;                                     //!< List of delayed buffers
    };

    varjo_Session* m_session = nullptr;                //!< Varjo session
    std::atomic_bool m_delayedBufferHandling = false;  //!< Flag for delayed buffer handling
    StreamData m_streamData;                           //!< Stream data
    ExposureAdjustments m_frameExposure;               //!< Latest known frame exposure adjustments (updated when color stream running)
    varjo_Matrix m_hmdPose;                            //!< Latest HMD pose
    CubemapFrame m_latestCubemapFrame;                 //!< Latest cubemap frame
    std::string m_statusLine;                          //!< Streaming status line

    //! Stream statistics
    struct {
        uint64_t streamCount{0};                                      //!< Stream count
        uint64_t frameCount{0};                                       //!< Frame count
        std::chrono::high_resolution_clock::time_point reportTime{};  //!< Last report time
    } m_stats;
};

}  // namespace VarjoExamples
