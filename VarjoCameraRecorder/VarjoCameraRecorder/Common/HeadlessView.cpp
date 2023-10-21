
#include "HeadlessView.hpp"

namespace VarjoExamples
{
HeadlessView::HeadlessView(varjo_Session* session)
    : SyncView(session, false)
{
}

}  // namespace VarjoExamples
