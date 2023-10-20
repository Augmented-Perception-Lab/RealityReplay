// Copyright 2019-2020 Varjo Technologies Oy. All rights reserved.

#include "ChromaKeyManager.hpp"

#include <cassert>
#include <sstream>
#include <iomanip>

namespace VarjoExamples
{
#define CHECK_CONFIG_LOCK                                   \
    if (!m_configLocked) {                                  \
        LOG_ERROR("Config lock required for this action."); \
    }

ChromaKeyManager::ChromaKeyManager(varjo_Session* session)
    : m_session(session)
{
    // Get chroma key config count
    m_count = varjo_MRGetChromaKeyConfigCount(m_session);
}

void ChromaKeyManager::print(const varjo_ChromaKeyConfig& config, const std::string& prefix)
{
    switch (config.type) {
        case varjo_ChromaKeyType_Disabled: {
            LOG_DEBUG("%s type=%s", prefix.c_str(), "Disabled");
        } break;
        case varjo_ChromaKeyType_HSV: {
            LOG_DEBUG("%s type=%s, color=(%.3f, %.2f, %.2f), tolerance=(%.2f, %.2f, %.2f), falloff=(%.2f, %.2f, %.2f)", prefix.c_str(),  //
                "HSV", config.params.hsv.targetColor[0], config.params.hsv.targetColor[1], config.params.hsv.targetColor[2],             //
                config.params.hsv.tolerance[0], config.params.hsv.tolerance[1], config.params.hsv.tolerance[2],                          //
                config.params.hsv.falloff[0], config.params.hsv.falloff[1], config.params.hsv.falloff[2]);
        } break;
        default: {
            LOG_DEBUG("%s type=Unknown(%d)", prefix.c_str(), static_cast<int>(config.type));
        } break;
    }
}

varjo_ChromaKeyConfig ChromaKeyManager::createConfigHSV(const glm::vec3& targetColor, const glm::vec3& tolerance, const glm::vec3& falloff)
{
    varjo_ChromaKeyConfig config{};
    config.type = varjo_ChromaKeyType_HSV;
    config.params.hsv.targetColor[0] = targetColor.x;
    config.params.hsv.targetColor[1] = targetColor.y;
    config.params.hsv.targetColor[2] = targetColor.z;
    config.params.hsv.tolerance[0] = tolerance.x;
    config.params.hsv.tolerance[1] = tolerance.y;
    config.params.hsv.tolerance[2] = tolerance.z;
    config.params.hsv.falloff[0] = falloff.x;
    config.params.hsv.falloff[1] = falloff.y;
    config.params.hsv.falloff[2] = falloff.z;
    return config;
}

varjo_ChromaKeyConfig ChromaKeyManager::createConfigDisabled()
{
    varjo_ChromaKeyConfig config{};
    config.type = varjo_ChromaKeyType_Disabled;
    return config;
}

bool ChromaKeyManager::lockConfig()
{
    LOG_DEBUG("Locking chroma key config.");

    if (m_configLocked) {
        LOG_WARNING("Config already locked.");
        return m_configLocked;
    }

    m_configLocked = varjo_Lock(m_session, varjo_LockType_ChromaKey);
    CHECK_VARJO_ERR(m_session);
    if (!m_configLocked) {
        LOG_ERROR("Getting chroma key config lock failed.");
    }

    return m_configLocked;
}

void ChromaKeyManager::unlockConfig()
{
    LOG_DEBUG("Unlocking chroma key config.");

    if (!m_configLocked) {
        LOG_WARNING("Config already unlocked.");
        return;
    }

    CHECK_CONFIG_LOCK;

    varjo_Unlock(m_session, varjo_LockType_ChromaKey);
    CHECK_VARJO_ERR(m_session);

    m_configLocked = false;
}

bool ChromaKeyManager::toggleChromaKeying(bool enabled)
{
    LOG_DEBUG("Toggle chroma keying: %s", enabled ? "ON" : "OFF");

    if (enabled == m_chromaKeyEnabled) {
        LOG_WARNING(enabled ? "Feature already enabled." : "Feature already disabled.");
        return false;
    }

    // Enable/disable chroma keying
    varjo_MRSetChromaKey(m_session, enabled ? varjo_True : varjo_False);
    if (CHECK_VARJO_ERR(m_session) == varjo_NoError) {
        m_chromaKeyEnabled = enabled;
        return true;
    }

    return false;
}

bool ChromaKeyManager::toggleGlobalChromaKeying(bool enabled)
{
    LOG_INFO("Toggle global chroma keying: %s", enabled ? "ON" : "OFF");

    if (enabled == m_globalChromaKeyEnabled) {
        LOG_WARNING(enabled ? "Feature already enabled." : "Feature already disabled.");
        return false;
    }

    // Enable/disable global chroma keying
    varjo_MRSetChromaKeyGlobal(m_session, enabled ? varjo_True : varjo_False);
    if (CHECK_VARJO_ERR(m_session) == varjo_NoError) {
        m_globalChromaKeyEnabled = enabled;
        return true;
    }

    return false;
}

varjo_ChromaKeyConfig ChromaKeyManager::getConfig(int index) const
{
    auto config = varjo_MRGetChromaKeyConfig(m_session, index);
    CHECK_VARJO_ERR(m_session);

    std::string prefix = "Got chromakey config (" + std::to_string(index) + "):";
    print(config, prefix);

    return config;
}

bool ChromaKeyManager::setConfig(int index, const varjo_ChromaKeyConfig& config)
{
    std::string prefix = "Set chromakey config (" + std::to_string(index) + "):";
    print(config, prefix);

    CHECK_CONFIG_LOCK;

    varjo_MRSetChromaKeyConfig(m_session, index, &config);

    return (CHECK_VARJO_ERR(m_session) == varjo_NoError);
}

}  // namespace VarjoExamples
