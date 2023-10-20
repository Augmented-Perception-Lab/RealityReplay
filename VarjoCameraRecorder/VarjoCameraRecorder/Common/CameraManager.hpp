// Copyright 2019-2020 Varjo Technologies Oy. All rights reserved.

#pragma once

#include <string>
#include <vector>

#include <Varjo_mr.h>

#include "Globals.hpp"

namespace VarjoExamples
{
//! Simple example class for managing Varjo mixed reality camera
class CameraManager
{
public:
    //! Construct camera manager
    CameraManager(varjo_Session* session);

    //! Convert given property type to string
    static std::string propertyTypeToString(varjo_CameraPropertyType propertyType, bool brief = false);

    //! Convert given property mode to string
    static std::string propertyModeToString(varjo_CameraPropertyMode propertyMode);

    //! Convert given property value to string
    static std::string propertyValueToString(varjo_CameraPropertyValue propertyValue);

    //! Print out currently applied camera configuration
    void printCurrentPropertyConfig() const;

    //! Print out all supported camera properties
    void printSupportedProperties() const;

    //! Set given property to auto mode
    void setAutoMode(varjo_CameraPropertyType propertyType);

    //! Set camera property to next available mode/value
    void applyNextModeOrValue(varjo_CameraPropertyType type);

    //! Reset all properties to default values
    void resetPropertiesToDefaults();

    //! Get camera property mode and value as string
    std::string getPropertyAsString(varjo_CameraPropertyType type) const;

    //! Return camera statusline
    const std::string& getStatusLine() const { return m_statusLine; }

    //! Update camera status
    void updateStatus();

private:
    //! Get list of available property modes for given property type
    std::vector<varjo_CameraPropertyMode> getPropertyModeList(varjo_CameraPropertyType propertyType) const;

    //! Get list of available property values for given property type
    std::vector<varjo_CameraPropertyValue> getPropertyValueList(varjo_CameraPropertyType propertyType) const;

    //! Print out supported property modes and values for given property type
    void printSupportedPropertyModesAndValues(varjo_CameraPropertyType propertyType) const;

    //! Find index for given mode in property modes list
    int findPropertyModeIndex(varjo_CameraPropertyMode mode, const std::vector<varjo_CameraPropertyMode>& modes) const;

    //! Find index for given value in property values list
    int findPropertyValueIndex(varjo_CameraPropertyValue propertyValue, const std::vector<varjo_CameraPropertyValue>& values) const;

    //! Set property value of given property type to given index (wrap around)
    void setPropertyValueToModuloIndex(varjo_CameraPropertyType propertyType, int index);

    //! Set property mode of given property type to given index (wrap around)
    void setPropertyModeToModuloIndex(varjo_CameraPropertyType type, int index);

private:
    varjo_Session* m_session{nullptr};      //!< Varjo session pointer
    std::string m_statusLine{"(Unknown)"};  //!< Statusline
};

}  // namespace VarjoExamples
