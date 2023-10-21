
#include "SyncView.hpp"

namespace VarjoExamples
{
SyncView::SyncView(varjo_Session* session, bool useFrameTiming)
    : m_session(session)
    , m_useFrameTiming(useFrameTiming)
{
    // Create a per-frame info structure
    m_frameInfo = varjo_CreateFrameInfo(m_session);
    CHECK_VARJO_ERR(m_session);

    // Reset timers
    m_startTime = m_lastFrameTime = varjo_GetCurrentTime(m_session);
}

SyncView::~SyncView()
{
    // These just free allocated resource, so no session or error check needed
    varjo_FreeFrameInfo(m_frameInfo);
}

void SyncView::syncFrame()
{
    // Sync the per-frame data.
    varjo_WaitSync(m_session, m_frameInfo);
    if (CHECK_VARJO_ERR(m_session) != varjo_NoError) {
        return;
    }

    // TODO: This is now different since the wait sync does not advance frame info display time
    // if the application does not submit frames. In that case we rely on current time call.

    if (m_useFrameTiming) {
        // Update frame timing
        m_deltaTime = static_cast<double>(m_frameInfo->displayTime - m_lastFrameTime) / 1e9;
        m_frameTime = static_cast<double>(m_frameInfo->displayTime) / 1e9;
        m_lastFrameTime = m_frameInfo->displayTime;
        m_frameNumber = m_frameInfo->frameNumber;
    } else {
        // Update timing from current time
        auto currentTime = varjo_GetCurrentTime(m_session);
        m_deltaTime = static_cast<double>(currentTime - m_lastFrameTime) / 1e9;
        m_frameTime = static_cast<double>(currentTime) / 1e9;
        m_lastFrameTime = currentTime;
        m_frameNumber++;
    }
}

}  // namespace VarjoExamples
