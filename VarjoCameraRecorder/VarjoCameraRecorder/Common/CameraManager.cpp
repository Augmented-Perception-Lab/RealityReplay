// Copyright 2019-2020 Varjo Technologies Oy. All rights reserved.

#include "CameraManager.hpp"

#include <cassert>
#include <sstream>
#include <iomanip>

namespace VarjoExamples
{
CameraManager::CameraManager(varjo_Session* session)
    : m_session(session)
{
}

std::string CameraManager::propertyTypeToString(varjo_CameraPropertyType propertyType, bool brief)
{
    switch (propertyType) {
        case varjo_CameraPropertyType_ExposureTime: return brief ? "Exp" : "Exposure Time";
        case varjo_CameraPropertyType_ISOValue: return brief ? "ISO" : "ISO Value";
        case varjo_CameraPropertyType_WhiteBalance: return brief ? "WB" : "White Balance";
        case varjo_CameraPropertyType_FlickerCompensation: return brief ? "Flick" : "Flicker Compensation";
        case varjo_CameraPropertyType_Sharpness: return brief ? "Sharp" : "Sharpness";
        default: assert(false); return "Unknown";
    }
}

std::string CameraManager::propertyModeToString(varjo_CameraPropertyMode propertyMode)
{
    switch (propertyMode) {
        case varjo_CameraPropertyMode_Off: return "Off";
        case varjo_CameraPropertyMode_Auto: return "Auto";
        case varjo_CameraPropertyMode_Manual: return "Manual";
        default: assert(false); return "Unknown";
    }
}

std::string CameraManager::propertyValueToString(varjo_CameraPropertyValue propertyValue)
{
    std::ostringstream ss;
    switch (propertyValue.type) {
        case varjo_CameraPropertyDataType_Bool: {
            // ss << "bool value ";
            ss << propertyValue.value.boolValue ? "true" : "false";
            break;
        }
        case varjo_CameraPropertyDataType_Int: {
            // ss << "int value ";
            ss << propertyValue.value.intValue;
            break;
        }
        case varjo_CameraPropertyDataType_Double: {
            // ss << "double value ";
            ss << std::fixed;
            ss << std::setprecision(2);
            ss << propertyValue.value.doubleValue;
            break;
        }
        default: {
            CRITICAL("Invalid type: %lld", propertyValue.type);
            break;
        }
    }
    return ss.str();
}

void CameraManager::printCurrentPropertyConfig() const
{
    LOG_INFO("\nCurrent camera config:");

    auto types = {varjo_CameraPropertyType_ExposureTime, varjo_CameraPropertyType_ISOValue, varjo_CameraPropertyType_WhiteBalance,
        varjo_CameraPropertyType_FlickerCompensation, varjo_CameraPropertyType_Sharpness};

    for (const auto& propertyType : types) {
        LOG_INFO("  %s: %s", propertyTypeToString(propertyType, false).c_str(), getPropertyAsString(propertyType).c_str());
    }
    LOG_INFO("");
}

void CameraManager::updateStatus()
{
    auto types = {varjo_CameraPropertyType_ExposureTime, varjo_CameraPropertyType_ISOValue, varjo_CameraPropertyType_WhiteBalance,
        varjo_CameraPropertyType_FlickerCompensation, varjo_CameraPropertyType_Sharpness};

    m_statusLine = "";
    int i = 0;
    for (const auto& propertyType : types) {
        m_statusLine += (i++ ? " " : "") + propertyTypeToString(propertyType, true) + "=" + getPropertyAsString(propertyType);
    }
}

void CameraManager::printSupportedProperties() const
{
    LOG_INFO("\nSupported camera properties:");
    printSupportedPropertyModesAndValues(varjo_CameraPropertyType_ExposureTime);
    printSupportedPropertyModesAndValues(varjo_CameraPropertyType_ISOValue);
    printSupportedPropertyModesAndValues(varjo_CameraPropertyType_WhiteBalance);
    printSupportedPropertyModesAndValues(varjo_CameraPropertyType_FlickerCompensation);
    printSupportedPropertyModesAndValues(varjo_CameraPropertyType_Sharpness);
    LOG_INFO("");
}

void CameraManager::setAutoMode(varjo_CameraPropertyType propertyType)
{
    auto modes = getPropertyModeList(propertyType);

    // Check that the desired camera mode is supported.
    if (std::find(modes.begin(), modes.end(), varjo_CameraPropertyMode_Auto) == modes.end()) {
        LOG_WARNING("Auto not supported for property: %s", propertyTypeToString(propertyType).c_str());
        return;
    }

    // Before calling MRCameraSet*-functions the configuration must be locked.
    // This returns false in case the lock can't be obtained. ie. someone else
    // is holding the lock.

    auto ret = varjo_Lock(m_session, varjo_LockType_Camera);
    CHECK_VARJO_ERR(m_session);
    if (ret == varjo_False) {
        LOG_ERROR("Could not change mixed reality camera settings.");
        return;
    }

    varjo_MRSetCameraPropertyMode(m_session, propertyType, varjo_CameraPropertyMode_Auto);
    CHECK_VARJO_ERR(m_session);

    // Unlock the camera configuration.
    // If we'd like to prevent anyone else changing the settings, it can be left locked.
    varjo_Unlock(m_session, varjo_LockType_Camera);
    CHECK_VARJO_ERR(m_session);

    updateStatus();
}

void CameraManager::applyNextModeOrValue(varjo_CameraPropertyType type)
{
    auto ret = varjo_Lock(m_session, varjo_LockType_Camera);
    CHECK_VARJO_ERR(m_session);
    if (ret == varjo_False) {
        LOG_ERROR("Could not change mixed reality camera settings.");
        return;
    }

    auto currentMode = varjo_MRGetCameraPropertyMode(m_session, type);
    auto supportedModes = getPropertyModeList(type);

    // Set the next manual value if current mode is manual and the last manual value isn't already set.
    if (currentMode == varjo_CameraPropertyMode_Manual) {
        auto currentValue = varjo_MRGetCameraPropertyValue(m_session, type);
        auto supportedValues = getPropertyValueList(type);
        int currentValueIndex = findPropertyValueIndex(currentValue, supportedValues);

        if (currentValueIndex == -1) {
            LOG_ERROR("Error finding current value: %d", currentValue);
            varjo_Unlock(m_session, varjo_LockType_Camera);
            CHECK_VARJO_ERR(m_session);
            return;
        }

        if (currentValueIndex + 1 < static_cast<int>(supportedValues.size())) {
            setPropertyValueToModuloIndex(type, currentValueIndex + 1);
            varjo_Unlock(m_session, varjo_LockType_Camera);
            CHECK_VARJO_ERR(m_session);
            return;
        }
    }

    // Otherwise set the next mode.
    int currentModeIndex = findPropertyModeIndex(currentMode, supportedModes);
    if (currentModeIndex == -1) {
        LOG_ERROR("Error finding current mode: %d", currentMode);
    } else {
        setPropertyModeToModuloIndex(type, currentModeIndex + 1);
    }

    varjo_Unlock(m_session, varjo_LockType_Camera);
    CHECK_VARJO_ERR(m_session);
}

void CameraManager::resetPropertiesToDefaults()
{
    auto ret = varjo_Lock(m_session, varjo_LockType_Camera);
    CHECK_VARJO_ERR(m_session);
    if (ret == varjo_False) {
        LOG_ERROR("Could not lock camera config for resetting camera properties.");
        return;
    }

    varjo_MRResetCameraProperties(m_session);
    CHECK_VARJO_ERR(m_session);

    varjo_Unlock(m_session, varjo_LockType_Camera);
    CHECK_VARJO_ERR(m_session);

    updateStatus();
}

std::vector<varjo_CameraPropertyMode> CameraManager::getPropertyModeList(varjo_CameraPropertyType propertyType) const
{
    std::vector<varjo_CameraPropertyMode> modes;
    modes.resize(varjo_MRGetCameraPropertyModeCount(m_session, propertyType));
    varjo_MRGetCameraPropertyModes(m_session, propertyType, modes.data(), static_cast<int32_t>(modes.size()));
    CHECK_VARJO_ERR(m_session);
    return modes;
}

std::vector<varjo_CameraPropertyValue> CameraManager::getPropertyValueList(varjo_CameraPropertyType propertyType) const
{
    // Get property config type
    varjo_CameraPropertyConfigType conf = varjo_MRGetCameraPropertyConfigType(m_session, propertyType);
    CHECK_VARJO_ERR(m_session);
    if (conf != varjo_CameraPropertyConfigType_List) {
        LOG_ERROR("Expected a property list.");
        return {};
    }

    // Get value count
    auto valueCount = varjo_MRGetCameraPropertyValueCount(m_session, propertyType);

    // Get property values
    std::vector<varjo_CameraPropertyValue> values(valueCount);
    varjo_MRGetCameraPropertyValues(m_session, propertyType, values.data(), static_cast<int32_t>(values.size()));
    CHECK_VARJO_ERR(m_session);
    return values;
}

void CameraManager::printSupportedPropertyModesAndValues(varjo_CameraPropertyType propertyType) const
{
    auto modes = getPropertyModeList(propertyType);
    auto values = getPropertyValueList(propertyType);
    LOG_INFO("\n  Camera property: %s", propertyTypeToString(propertyType).c_str());

    {
        std::stringstream ss;
        if (!modes.empty()) {
            for (size_t i = 0; i < modes.size(); i++) {
                const auto& mode = modes[i];
                ss << (i == 0 ? "" : ", ") << propertyModeToString(mode).c_str() << "(" << mode << ")";
            }
        } else {
            ss << "(none)";
        }
        LOG_INFO("    Modes (%d): %s", static_cast<int>(modes.size()), ss.str().c_str());
    }

    {
        std::stringstream ss;
        if (!values.empty()) {
            for (size_t i = 0; i < values.size(); i++) {
                const auto& value = values[i];
                ss << (i == 0 ? "" : ", ") << propertyValueToString(value).c_str();
            }
        } else {
            ss << "(none)";
        }
        LOG_INFO("    Manual values (%d): %s", static_cast<int>(values.size()), ss.str().c_str());
    }
}

std::string CameraManager::getPropertyAsString(varjo_CameraPropertyType type) const
{
    auto mode = varjo_MRGetCameraPropertyMode(m_session, type);
    CHECK_VARJO_ERR(m_session);

    if (mode == varjo_CameraPropertyMode_Manual) {
        auto propVal = varjo_MRGetCameraPropertyValue(m_session, type);
        CHECK_VARJO_ERR(m_session);

        return propertyValueToString(propVal);
    }
    return propertyModeToString(mode);
}

int CameraManager::findPropertyModeIndex(varjo_CameraPropertyMode mode, const std::vector<varjo_CameraPropertyMode>& modes) const
{
    auto it = std::find(modes.begin(), modes.end(), mode);
    return it == modes.end() ? -1 : static_cast<int>(std::distance(modes.begin(), it));
}

int CameraManager::findPropertyValueIndex(varjo_CameraPropertyValue propertyValue, const std::vector<varjo_CameraPropertyValue>& values) const
{
    for (int i = 0; i < static_cast<int>(values.size()); ++i) {
        auto val = values[i];
        switch (propertyValue.type) {
            case varjo_CameraPropertyDataType_Bool:
                if (propertyValue.value.boolValue == val.value.boolValue) {
                    return i;
                }
                break;
            case varjo_CameraPropertyDataType_Double:
                if (propertyValue.value.doubleValue == val.value.doubleValue) {
                    return i;
                }
                break;
            case varjo_CameraPropertyDataType_Int:
                if (propertyValue.value.intValue == val.value.intValue) {
                    return i;
                }
                break;
        }
    }
    return -1;
}

void CameraManager::setPropertyValueToModuloIndex(varjo_CameraPropertyType propertyType, int index)
{
    const auto supportedValues = getPropertyValueList(propertyType);
    const varjo_CameraPropertyValue nextPropertyValue = supportedValues[index % supportedValues.size()];
    LOG_INFO("Setting the camera property manual value to: %s", propertyValueToString(nextPropertyValue).c_str());
    varjo_MRSetCameraPropertyValue(m_session, propertyType, &nextPropertyValue);
    CHECK_VARJO_ERR(m_session);

    updateStatus();
}

void CameraManager::setPropertyModeToModuloIndex(varjo_CameraPropertyType type, int index)
{
    const auto supportedModes = getPropertyModeList(type);
    const varjo_CameraPropertyMode nextPropertyMode = supportedModes[index % supportedModes.size()];
    LOG_INFO("Setting the camera property mode to: %s", propertyModeToString(nextPropertyMode).c_str());
    if (nextPropertyMode == varjo_CameraPropertyMode_Manual) {
        setPropertyValueToModuloIndex(type, 0);
    }
    varjo_MRSetCameraPropertyMode(m_session, type, nextPropertyMode);
    CHECK_VARJO_ERR(m_session);

    updateStatus();
}

}  // namespace VarjoExamples
