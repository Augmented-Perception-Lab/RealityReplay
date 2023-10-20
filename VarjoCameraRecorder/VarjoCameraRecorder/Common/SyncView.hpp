// Copyright 2020 Varjo Technologies Oy. All rights reserved.

#pragma once

#include <Varjo_layers.h>
#include <Varjo_types_layers.h>

#include "Globals.hpp"

namespace VarjoExamples
{
//! Base class for synchronized varjo views.
class SyncView
{
public:
    //! Virtual destructor
    virtual ~SyncView();

    //! Called to sync varjo frame before rendering
    virtual void syncFrame();

    //! Returns current frame time after sync
    double getFrameTime() const { return m_frameTime; }

    //! Returns delta time to previous frame after sync
    double getDeltaTime() const { return m_deltaTime; }

    //! Returns current frame number after sync
    int64_t getFrameNumber() const { return m_frameNumber; }

    //! Returns varjo session pointer
    varjo_Session* getSession() const { return m_session; }

protected:
    //! Protected costructor.
    SyncView(varjo_Session* session, bool useFrameTiming);

    //! Returns varjo frame info pointer
    const varjo_FrameInfo* getFrameInfo() const { return m_frameInfo; }

private:
    varjo_Session* m_session = nullptr;      //!< Session pointer
    varjo_FrameInfo* m_frameInfo = nullptr;  //!< Frame info pointer
    varjo_Nanoseconds m_startTime = 0;       //!< Start time
    varjo_Nanoseconds m_lastFrameTime = 0;   //!< Time of last updated frame
    double m_deltaTime = 0.0;                //!< Current frame delta in seconds
    double m_frameTime = 0.0;                //!< Current frame time in seconds
    int64_t m_frameNumber = 0;               //!< Frame counter
    bool m_useFrameTiming = false;           //!< Flag to enable timing from frame info
};

}  // namespace VarjoExamples