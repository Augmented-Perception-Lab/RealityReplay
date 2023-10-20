
#include "MarkerTracker.hpp"

#include <chrono>
#include <Varjo_types_world.h>

namespace
{
using namespace VarjoExamples;

// Use the marker range from https://developer.varjo.com/docs/mixed-reality/varjo-markers
constexpr std::pair<MarkerTracker::MarkerId, MarkerTracker::MarkerId> c_markerIdRange(100, 499);

}  // namespace

namespace VarjoExamples
{
MarkerTracker::MarkerTracker(varjo_Session* session)
    : m_session(session)
{
    // Initialize Varjo world with visual marker tracking enabled.
    m_world = varjo_WorldInit(m_session, varjo_WorldFlag_UseObjectMarkers);
    CHECK_VARJO_ERR(m_session);

    // Set default lifetime for all markers
    setLifetime(2.0);

    // Disable prediction for all markers
    setPrediction(false);
}

MarkerTracker::~MarkerTracker()
{
    // Destroy the Varjo world instance.
    varjo_WorldDestroy(m_world);
    CHECK_VARJO_ERR(m_session);

    m_world = nullptr;
}

const std::pair<MarkerTracker::MarkerId, MarkerTracker::MarkerId>& MarkerTracker::getMarkerIdRange() { return c_markerIdRange; }

bool MarkerTracker::isValidId(MarkerId id) { return (id >= c_markerIdRange.first && id <= c_markerIdRange.second); }

void MarkerTracker::setLifetime(double lifetime, const std::vector<MarkerId>& ids)
{
    std::vector<varjo_WorldMarkerId> markers = ids;
    if (markers.empty()) {
        for (auto id = c_markerIdRange.first; id <= c_markerIdRange.second; id++) {
            markers.emplace_back(id);
        }
    }

    // Lifetime in nanoseconds
    constexpr std::chrono::nanoseconds c_oneSecond_ns = std::chrono::seconds{1};
    varjo_Nanoseconds lifetime_ns = static_cast<varjo_Nanoseconds>(c_oneSecond_ns.count() * lifetime);

    // Set lifetime for markers
    varjo_WorldSetObjectMarkerTimeouts(m_world, markers.data(), markers.size(), lifetime_ns);
    CHECK_VARJO_ERR(m_session);
}

void MarkerTracker::setPrediction(bool enabled, const std::vector<MarkerId>& ids)
{
    std::vector<varjo_WorldMarkerId> markers = ids;
    if (markers.empty()) {
        for (auto id = c_markerIdRange.first; id <= c_markerIdRange.second; id++) {
            markers.emplace_back(id);
        }
    }

    // Set prediction flag for markers
    varjo_WorldObjectMarkerFlags flags = enabled ? varjo_WorldObjectMarkerFlags_DoPrediction : 0;
    varjo_WorldSetObjectMarkerFlags(m_world, markers.data(), markers.size(), flags);
    CHECK_VARJO_ERR(m_session);
}

const MarkerTracker::MarkerObject* MarkerTracker::getObject(MarkerId id) const
{
    if (!isValidId(id)) {
        LOG_ERROR("Invalid marker id: %d", id);
        return nullptr;
    }

    auto it = m_markers.find(id);
    if (it != m_markers.end()) {
        return &it->second;
    }
    return nullptr;
}

const MarkerTracker::MarkerMap& MarkerTracker::getObjects() const { return m_markers; }

void MarkerTracker::reset()
{
    // Reset all marker data
    m_markers.clear();
}

void MarkerTracker::update()
{
    // Update the tracking data for visual markers.
    varjo_WorldSync(m_world);

    // Get Varjo frame display timestamp
    const varjo_Nanoseconds displayTime = varjo_FrameGetDisplayTime(m_session);
    CHECK_VARJO_ERR(m_session);

    // Get object count
    const auto objectMask = varjo_WorldComponentTypeMask_Pose | varjo_WorldComponentTypeMask_ObjectMarker;
    const int64_t objectCount = varjo_WorldGetObjectCount(m_world, objectMask);
    CHECK_VARJO_ERR(m_session);

    // LOG_DEBUG("Object count: %d", objectCount);

    if (objectCount > 0) {
        // Allocate data for all components.
        std::vector<varjo_WorldObject> objects(static_cast<size_t>(objectCount));

        // Get objects
        varjo_WorldGetObjects(m_world, objects.data(), objectCount, objectMask);
        CHECK_VARJO_ERR(m_session);

        // Update markers
        for (const auto& object : objects) {
            // Get the pose component
            varjo_WorldPoseComponent pose{};
            varjo_WorldGetPoseComponent(m_world, object.id, &pose, displayTime);
            CHECK_VARJO_ERR(m_session);

            // Get the object marker component
            varjo_WorldObjectMarkerComponent marker{};
            varjo_WorldGetObjectMarkerComponent(m_world, object.id, &marker);
            CHECK_VARJO_ERR(m_session);

            // marker.error
            if (marker.error == varjo_WorldObjectMarkerError_None) {
                // Copy the pose matrix
                glm::mat4x4 matrix = fromVarjoMatrix(pose.pose);

                // Add marker object
                MarkerObject markerObj;
                markerObj.id = marker.id;
                markerObj.time = displayTime;
                markerObj.pose = matrix;
                markerObj.size = fromVarjoSize(marker.size);

                m_markers[markerObj.id] = markerObj;
            }
        }
    }
}

}  // namespace VarjoExamples
