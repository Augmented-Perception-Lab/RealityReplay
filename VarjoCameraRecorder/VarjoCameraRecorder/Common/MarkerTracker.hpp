// Copyright 2020 Varjo Technologies Oy. All rights reserved.

#pragma once

#include <vector>
#include <memory>
#include <array>
#include <unordered_map>

#include <Varjo.h>
#include <Varjo_world.h>

#include "Globals.hpp"

namespace VarjoExamples
{
//! Wrapper for Varjo World API to demonstrate tracking visual markers
class MarkerTracker
{
public:
    //! Marker id type
    using MarkerId = int64_t;

    //! Struct for storing marker data.
    struct MarkerObject {
        varjo_Nanoseconds time{0};  //!< Update timestamp
        glm::mat4x4 pose{1.0f};     //!< Marker pose matrix
        glm::vec3 size{1.0f};       //!< Marker size
        MarkerId id = 0;            //!< Marker id
    };

    //! Map of currently known markers
    using MarkerMap = std::unordered_map<MarkerId, MarkerObject>;

    //! Constructor
    MarkerTracker(varjo_Session* session);

    //! Destructor
    ~MarkerTracker();

    //! Return the marker id range
    static const std::pair<MarkerId, MarkerId>& getMarkerIdRange();

    //! Return true if given id is in valid range
    static bool isValidId(MarkerId id);

    //! Set timeout to all markers (optionally for specific markers)
    void setLifetime(double lifetime, const std::vector<MarkerId>& ids = {});

    //! Set prediction enabled/disabled (optionally for specific markers)
    void setPrediction(bool enabled, const std::vector<MarkerId>& ids = {});

    //! Reset markers
    void reset();

    //! Update markers
    void update();

    //! Get object by id
    const MarkerObject* getObject(MarkerId id) const;

    //! Return map of all known objects
    const MarkerMap& getObjects() const;

private:
    varjo_Session* m_session = nullptr;  //!< Varjo session instance
    varjo_World* m_world = nullptr;      //!< Varjo world instance
    MarkerMap m_markers;                 //!< List of detected markers
};

}  // namespace VarjoExamples
