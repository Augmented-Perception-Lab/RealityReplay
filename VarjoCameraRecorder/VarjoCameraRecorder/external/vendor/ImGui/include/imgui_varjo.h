#pragma once

#include "imgui.h"
#ifndef IMGUI_DISABLE

namespace ImGui
{
namespace Varjo
{
    IMGUI_API bool BeginCombo(const char* label, const char* preview_value, ImGuiComboFlags flags, const ImVec2& size, const ImVec2& textMargin, ImU32 textHoverColor, bool& hovered);
    IMGUI_API bool Selectable(const char* label, bool selected, ImGuiSelectableFlags flags, const ImVec2& size_arg, const ImVec2& textMargin, ImU32 textHoverColor);
}  // namespace Varjo
}  // namespace ImGui
#endif // #ifndef IMGUI_DISABLE
