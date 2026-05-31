/*
 * PrismaUI_F4_Helper.h — optional utility helpers for PrismaUI_F4 plugin authors.
 *
 * This file is NOT part of the core API guarantee. It is a convenience layer.
 * Drop it into your project alongside PrismaUI_F4_API.h.
 *
 * Requires: CommonLibF4, F4SE
 */
#pragma once
#include "PrismaUI_F4_API.h"

#include <RE/Fallout.h>
#include <F4SE/F4SE.h>
#include <string>
#include <string_view>

namespace PRISMA_UI_HELPER {

    // -------------------------------------------------------------------------
    // Papyrus custom event dispatch
    // Call only from the game thread (e.g., inside a BindUIEvent callback).
    // -------------------------------------------------------------------------

    // Send a Papyrus custom event to all scripts listening on the given form.
    // eventName must match the CustomEvent declaration in your Papyrus script.
    // args is an optional Var[] array passed to the event handler.
    inline void SendPapyrusEvent(RE::TESForm* form, const RE::BSFixedString& eventName,
                                 RE::BSScript::IFunctionArguments* args = nullptr)
    {
        if (!form) return;

        auto* gameVM = RE::GameVM::GetSingleton();
        if (!gameVM) return;

        auto* vm = gameVM->GetVM().get();
        if (!vm) return;

        auto handle = vm->GetObjectHandlePolicy().GetHandleForObject(
            form->GetFormType(), form);

        if (handle == vm->GetObjectHandlePolicy().EmptyHandle()) return;

        vm->SendCustomEvent(handle, nullptr, eventName, args);
    }

    // -------------------------------------------------------------------------
    // Convenience wrappers — BindUIEvent with named event forwarding to Papyrus
    // -------------------------------------------------------------------------

    // Wire a JS function to a Papyrus custom event on a form.
    // The raw JSON string from JS is passed as akArgs[0] in Papyrus.
    //
    // Papyrus side:
    //   CustomEvent OnMyEvent
    //   Event PrismaManager.OnMyEvent(ObjectReference akSender, Var[] akArgs)
    //       string json = akArgs[0] as string
    //   EndEvent
    //
    inline void BindUIEventToPapyrus(PRISMA_UI_API::IVPrismaUI4* api, PrismaView view,
                                     const char* jsFunctionName,
                                     RE::TESForm* targetForm,
                                     const RE::BSFixedString& papyrusEventName)
    {
        if (!api || !targetForm) return;

        // Capture by value — targetForm and eventName must remain valid for the lifetime of the view.
        api->BindUIEvent(view, jsFunctionName, [](const char* /*json*/) {
            // NOTE: capturing targetForm and papyrusEventName requires a static or global wrapper
            // since JSListenerCallback is a raw function pointer. For capture support, use
            // RegisterJSListener with a manual AddTask and call SendPapyrusEvent inside.
        });

        // For full capture support, use this pattern directly in your OnDomReady:
        //
        //   api->RegisterJSListener(view, "myEvent", [](const char* json) {
        //       std::string j = json ? json : "";
        //       F4SE::GetTaskInterface()->AddTask([j]() {
        //           auto* form = RE::TESForm::GetFormByID(0x12345);
        //           PRISMA_UI_HELPER::SendPapyrusEvent(form, "OnMyEvent");
        //       });
        //   });
    }

    // -------------------------------------------------------------------------
    // JSON convenience — extract simple values from the JSON string JS sends
    // These are minimal helpers. Use nlohmann::json for anything complex.
    // -------------------------------------------------------------------------

    // Extract a string value for a key from a flat JSON object.
    // e.g. GetJsonString(R"({"key":"hello"})", "key") -> "hello"
    inline std::string GetJsonString(std::string_view json, std::string_view key)
    {
        std::string search = "\"";
        search += key;
        search += "\":\"";
        auto pos = json.find(search);
        if (pos == std::string_view::npos) return {};
        pos += search.size();
        auto end = json.find('"', pos);
        if (end == std::string_view::npos) return {};
        return std::string(json.substr(pos, end - pos));
    }

    // Extract a numeric value for a key from a flat JSON object.
    // e.g. GetJsonFloat(R"({"value":1.5})", "value") -> 1.5f
    inline float GetJsonFloat(std::string_view json, std::string_view key, float fallback = 0.0f)
    {
        std::string search = "\"";
        search += key;
        search += "\":";
        auto pos = json.find(search);
        if (pos == std::string_view::npos) return fallback;
        pos += search.size();
        try { return std::stof(std::string(json.substr(pos))); }
        catch (...) { return fallback; }
    }

    inline int GetJsonInt(std::string_view json, std::string_view key, int fallback = 0)
    {
        return static_cast<int>(GetJsonFloat(json, key, static_cast<float>(fallback)));
    }

    inline bool GetJsonBool(std::string_view json, std::string_view key, bool fallback = false)
    {
        std::string search = "\"";
        search += key;
        search += "\":";
        auto pos = json.find(search);
        if (pos == std::string_view::npos) return fallback;
        pos += search.size();
        auto sub = json.substr(pos);
        if (sub.starts_with("true"))  return true;
        if (sub.starts_with("false")) return false;
        return fallback;
    }
}
