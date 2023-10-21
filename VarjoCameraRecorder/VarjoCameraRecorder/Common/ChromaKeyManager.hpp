// Copyright 2019-2020 Varjo Technologies Oy. All rights reserved.

#pragma once

#include <string>
#include <vector>
#include <glm/vec3.hpp>

#include <Varjo_mr.h>

#include "Globals.hpp"

namespace VarjoExamples
{
//! Simple example class for managing Varjo mixed reality chroma key configurations
class ChromaKeyManager
{
public:
    //! Construct chroma key manager
    ChromaKeyManager(varjo_Session* session);

    //! Default virtual destructor
    virtual ~ChromaKeyManager() = default;

    //! Return config count
    int getCount() const { return m_count; }

    //! Lock chroma key config
    bool lockConfig();

    //! Unlock chroma key config
    void unlockConfig();

    //! Returns if config is currently locked
    bool isConfigLocked() const { return m_configLocked; }

    //! Toggle chroma keying on/off
    bool toggleChromaKeying(bool enabled);

    //! Returns true if chroma keying has been enabled
    bool isChromaKeyingEnabled() const { return m_chromaKeyEnabled; }

    //! Toggle global chroma keying override on/off
    bool toggleGlobalChromaKeying(bool enabled);

    //! Return true if global chroma keying enabled
    bool isGlobalChromaKeyingEnabled() const { return m_globalChromaKeyEnabled; }

    //! Get config parameters
    varjo_ChromaKeyConfig getConfig(int index) const;

    //! Set config parameters
    bool setConfig(int index, const varjo_ChromaKeyConfig& config);

    //! Static helper to print out given config
    static void print(const varjo_ChromaKeyConfig& config, const std::string& prefix = "ChromaKey config:");

    //! Static helper to create HSV config
    static varjo_ChromaKeyConfig createConfigHSV(const glm::vec3& targetColor, const glm::vec3& tolerance, const glm::vec3& falloff);

    //! Static helper to create disabled config
    static varjo_ChromaKeyConfig createConfigDisabled();

private:
    varjo_Session* m_session = nullptr;     //!< Varjo session pointer
    bool m_configLocked = false;            //!< Config lock flag
    bool m_chromaKeyEnabled = false;        //!< Chroma keying running flag
    bool m_globalChromaKeyEnabled = false;  //!< Global chroma key running flag
    int m_count = 0;                        //!< Config count
};

}  // namespace VarjoExamples
