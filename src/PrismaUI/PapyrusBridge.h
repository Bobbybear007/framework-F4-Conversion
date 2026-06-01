#pragma once

#pragma warning(push)
#pragma warning(disable : 4100)
#include <Ultralight/View.h>
#pragma warning(pop)

#include "Core.h"

namespace PrismaUI::PapyrusBridge {

// Injects window.prisma into a view.
// Must be called from OnWindowObjectReady (Ultralight thread, inside callback).
void InjectBridge(ultralight::View* caller, Core::PrismaViewId viewId);

} // namespace PrismaUI::PapyrusBridge
