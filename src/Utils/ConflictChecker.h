#pragma once

#include "PrismaUI_F4_API.h"

namespace PrismaUI::ConflictChecker {

    // Call in F4SEPlugin_Load — before any other init.
    // Checks: duplicate PrismaUI_F4.dll instances, RequestPluginAPI export squatting.
    void CheckEarly();

    // Call in kGameDataReady handler — before D3DHooks::Install.
    // Checks: known conflicting DLLs, D3D vtable integrity, prints API consumer summary.
    void CheckPreHooks();

    // Call at the top of RequestPluginAPI — before the interface switch.
    // Tracks which plugin requested which interface version; logs mismatches.
    void OnAPIRequest(void* returnAddress, PRISMA_UI_API::InterfaceVersion version);

}
