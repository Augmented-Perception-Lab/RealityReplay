// Copyright 2020 Varjo Technologies Oy. All rights reserved.

#pragma once

#include <Varjo_layers.h>
#include <Varjo_types_layers.h>

#include "Globals.hpp"
#include "SyncView.hpp"

namespace VarjoExamples
{
//! Headless view class that can be used for sync and timing if app is not rendering.
class HeadlessView final : public SyncView
{
public:
    //! Constructs view
    HeadlessView(varjo_Session* session);

    //! Default destructor
    ~HeadlessView() = default;
};

}  // namespace VarjoExamples