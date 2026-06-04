#pragma once

#include "Core.h"

namespace PrismaUI::NotificationSystem {
    // Show an overlay notification banner at the top of the screen
    // title: displayed in bold
    // message: main content
    // duration: milliseconds to display (0 = persist until dismissed)
    // color: "warning" (yellow), "error" (red), "info" (blue), "success" (green)
    Core::PrismaViewId ShowNotification(const std::string& title, const std::string& message,
                                        uint32_t duration = 8000, const std::string& color = "warning");

    // Dismiss a notification by view ID
    void DismissNotification(Core::PrismaViewId notifId);

    // Create and show the overlay conflict warning
    void ShowOverlayConflictWarning();
}
