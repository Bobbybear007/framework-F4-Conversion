#include "PapyrusBridge.h"

#include "Communication.h"
#include "ViewManager.h"

#pragma warning(push)
#pragma warning(disable : 4100)
#include <JavaScriptCore/JSBase.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSValueRef.h>
#pragma warning(pop)

namespace PrismaUI::PapyrusBridge {

// Builds window.prisma and communicates via __prisma_request
static constexpr const char* kBridgeScript = R"js(
(function() {
    'use strict';
    var _pending = {};
    var _nextId = 1;

    window.__prisma_resolve = function(id, value) {
        var cb = _pending[id];
        if (cb) { delete _pending[id]; cb.resolve(value); }
    };

    window.__prisma_reject = function(id, msg) {
        var cb = _pending[id];
        if (cb) { delete _pending[id]; cb.reject(new Error(String(msg))); }
    };

    function _read(op, params) {
        return new Promise(function(resolve, reject) {
            var id = String(_nextId++);
            _pending[id] = { resolve: resolve, reject: reject };
            try {
                window.__prisma_request(JSON.stringify(
                    Object.assign({ op: op, id: id }, params)
                ));
            } catch(e) { delete _pending[id]; reject(e); }
        });
    }

    function _write(op, params) {
        try {
            window.__prisma_request(JSON.stringify(
                Object.assign({ op: op, id: '' }, params)
            ));
        } catch(e) {}
    }

    window.prisma = Object.freeze({
        getGlobal: function(esp, formId) {
            return _read('getGlobal', { esp: String(esp), formId: String(formId) });
        },
        setGlobal: function(esp, formId, value) {
            _write('setGlobal', { esp: String(esp), formId: String(formId), value: +value });
        },
        getProperty: function(esp, formId, scriptName, propName) {
            return _read('getProperty', {
                esp: String(esp), formId: String(formId),
                scriptName: String(scriptName), propName: String(propName)
            });
        },
        setProperty: function(esp, formId, scriptName, propName, value) {
            _write('setProperty', {
                esp: String(esp), formId: String(formId),
                scriptName: String(scriptName), propName: String(propName),
                value: value
            });
        }
    });
})();
)js";

// Minimal JSON field extractors (avoids external JSON library dependency)
static std::string JsonGetString(const std::string& json, const std::string& key) {
    auto kp = "\"" + key + "\":\"";
    auto p  = json.find(kp);
    if (p == std::string::npos) return {};
    p += kp.size();
    auto e = json.find('"', p);
    return e == std::string::npos ? std::string{} : json.substr(p, e - p);
}

static double JsonGetDouble(const std::string& json, const std::string& key) {
    auto kp = "\"" + key + "\":";
    auto p  = json.find(kp);
    if (p == std::string::npos) return 0.0;
    p += kp.size();
    try { return std::stod(json.substr(p)); } catch (...) { return 0.0; }
}

// Game data helpers (run on game thread only)
// Resolve (esp, formIdHex) to a TESForm*
static RE::TESForm* LookupFormByPlugin(const std::string& esp, const std::string& formIdHex) {
    uint32_t localId = 0;
    try { localId = std::stoul(formIdHex, nullptr, 16); } catch (...) { return nullptr; }

    auto* handler = RE::TESDataHandler::GetSingleton();
    if (!handler) return nullptr;

    const RE::TESFile* mod = handler->LookupModByName(esp.c_str());
    if (!mod) return nullptr;

    uint32_t fullId;
    if (mod->IsLight()) {
        fullId = 0xFE000000u
               | (static_cast<uint32_t>(mod->GetSmallFileCompileIndex()) << 12u)
               | (localId & 0xFFFu);
    } else {
        fullId = (static_cast<uint32_t>(mod->GetCompileIndex()) << 24u)
               | (localId & 0x00FFFFFFu);
    }
    return RE::TESForm::GetFormByID(fullId);
}

struct PropResult {
    bool   found    = false;
    bool   isBool   = false;
    bool   boolVal  = false;
    double numVal   = 0.0;
};

static PropResult GetPapyrusProperty(RE::TESForm* form,
                                     const std::string& scriptName,
                                     const std::string& propName) {
    if (!form) return {};
    auto* gameVM = RE::GameVM::GetSingleton();
    if (!gameVM) return {};
    auto* vm = gameVM->GetVM().get();
    if (!vm) return {};
    auto* concreteVM = static_cast<RE::BSScript::Internal::VirtualMachine*>(vm);

    RE::BSAutoLock lock(concreteVM->attachedScriptsLock);

    auto  handle = vm->GetObjectHandlePolicy().GetHandleForObject(
                       static_cast<uint32_t>(form->GetFormType()), form);
    auto  it     = concreteVM->attachedScripts.find(handle);
    if (it == concreteVM->attachedScripts.end()) return {};

    for (auto& attached : it->second) {
        auto* obj = attached.get();
        if (!obj) continue;
        if (!scriptName.empty() &&
            _stricmp(obj->GetTypeInfo()->GetName(), scriptName.c_str()) != 0)
            continue;

        auto* prop = obj->GetProperty(RE::BSFixedString(propName.c_str()));
        if (!prop) continue;

        PropResult r;
        r.found = true;
        if (prop->is<bool>()) {
            r.isBool  = true;
            r.boolVal = RE::BSScript::get<bool>(*prop);
        } else if (prop->is<float>()) {
            r.numVal = static_cast<double>(RE::BSScript::get<float>(*prop));
        } else if (prop->is<std::int32_t>()) {
            r.numVal = static_cast<double>(RE::BSScript::get<std::int32_t>(*prop));
        }
        return r;
    }
    return {};
}

static bool SetPapyrusProperty(RE::TESForm* form,
                                const std::string& scriptName,
                                const std::string& propName,
                                double value) {
    if (!form) return false;
    auto* gameVM = RE::GameVM::GetSingleton();
    if (!gameVM) return false;
    auto* vm = gameVM->GetVM().get();
    if (!vm) return false;
    auto* concreteVM = static_cast<RE::BSScript::Internal::VirtualMachine*>(vm);

    RE::BSAutoLock lock(concreteVM->attachedScriptsLock);

    auto handle = vm->GetObjectHandlePolicy().GetHandleForObject(
                      static_cast<uint32_t>(form->GetFormType()), form);
    auto it     = concreteVM->attachedScripts.find(handle);
    if (it == concreteVM->attachedScripts.end()) return false;

    for (auto& attached : it->second) {
        auto* obj = attached.get();
        if (!obj) continue;
        if (!scriptName.empty() &&
            _stricmp(obj->GetTypeInfo()->GetName(), scriptName.c_str()) != 0)
            continue;

        auto* prop = obj->GetProperty(RE::BSFixedString(propName.c_str()));
        if (!prop) continue;

        if (prop->is<float>())         *prop = static_cast<float>(value);
        else if (prop->is<std::int32_t>()) *prop = static_cast<std::int32_t>(value);
        else if (prop->is<bool>())     *prop = (value != 0.0);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Build a __prisma_resolve / __prisma_reject JS call string
// ---------------------------------------------------------------------------
static std::string BuildResolve(const std::string& id, const PropResult& r) {
    if (!r.found)
        return "__prisma_resolve('" + id + "',null)";
    if (r.isBool)
        return "__prisma_resolve('" + id + "'," + (r.boolVal ? "true" : "false") + ")";
    return "__prisma_resolve('" + id + "'," + std::to_string(r.numVal) + ")";
}

// ---------------------------------------------------------------------------
// Native JSC callback — fires on the Ultralight thread
// ---------------------------------------------------------------------------
static JSValueRef PrismaRequestCallback(JSContextRef ctx, JSObjectRef function,
                                         JSObjectRef /*thisObject*/,
                                         size_t argc, const JSValueRef argv[],
                                         JSValueRef* exception) {
    if (argc == 0) return JSValueMakeUndefined(ctx);

    // Extract JSON argument
    JSStringRef jsStr = JSValueToStringCopy(ctx, argv[0], exception);
    if (!jsStr) return JSValueMakeUndefined(ctx);
    size_t bufSz = JSStringGetMaximumUTF8CStringSize(jsStr);
    std::string req(bufSz, '\0');
    JSStringGetUTF8CString(jsStr, req.data(), bufSz);
    JSStringRelease(jsStr);
    req.resize(strlen(req.c_str()));

    // Extract viewId stored as a property on this function object
    JSStringRef vidKey = JSStringCreateWithUTF8CString("viewId");
    JSValueRef  vidVal = JSObjectGetProperty(ctx, function, vidKey, nullptr);
    JSStringRelease(vidKey);

    JSStringRef vidStr = JSValueToStringCopy(ctx, vidVal, nullptr);
    if (!vidStr) return JSValueMakeUndefined(ctx);
    size_t vSz = JSStringGetMaximumUTF8CStringSize(vidStr);
    std::string vidS(vSz, '\0');
    JSStringGetUTF8CString(vidStr, vidS.data(), vSz);
    JSStringRelease(vidStr);
    vidS.resize(strlen(vidS.c_str()));

    Core::PrismaViewId viewId = 0;
    try { viewId = std::stoull(vidS); } catch (...) { return JSValueMakeUndefined(ctx); }

    std::string op         = JsonGetString(req, "op");
    std::string callbackId = JsonGetString(req, "id");

    // ---- getGlobal ----
    if (op == "getGlobal") {
        std::string esp    = JsonGetString(req, "esp");
        std::string formId = JsonGetString(req, "formId");

        F4SE::GetTaskInterface()->AddTask([viewId, callbackId, esp, formId]() {
            if (!ViewManager::IsValid(viewId)) return;
            auto* form   = LookupFormByPlugin(esp, formId);
            auto* global = form ? form->As<RE::TESGlobal>() : nullptr;
            std::string script = global
                ? "__prisma_resolve('" + callbackId + "'," + std::to_string(global->value) + ")"
                : "__prisma_resolve('" + callbackId + "',null)";
            Communication::Invoke(viewId, ultralight::String(script.c_str()), nullptr);
        });

    // ---- setGlobal ----
    } else if (op == "setGlobal") {
        std::string esp    = JsonGetString(req, "esp");
        std::string formId = JsonGetString(req, "formId");
        double      value  = JsonGetDouble(req, "value");

        F4SE::GetTaskInterface()->AddTask([esp, formId, value]() {
            auto* form   = LookupFormByPlugin(esp, formId);
            auto* global = form ? form->As<RE::TESGlobal>() : nullptr;
            if (global) global->value = static_cast<float>(value);
        });

    // ---- getProperty ----
    } else if (op == "getProperty") {
        std::string esp        = JsonGetString(req, "esp");
        std::string formId     = JsonGetString(req, "formId");
        std::string scriptName = JsonGetString(req, "scriptName");
        std::string propName   = JsonGetString(req, "propName");

        F4SE::GetTaskInterface()->AddTask([viewId, callbackId, esp, formId, scriptName, propName]() {
            if (!ViewManager::IsValid(viewId)) return;
            auto* form   = LookupFormByPlugin(esp, formId);
            auto  result = GetPapyrusProperty(form, scriptName, propName);
            std::string script = BuildResolve(callbackId, result);
            Communication::Invoke(viewId, ultralight::String(script.c_str()), nullptr);
        });

    // ---- setProperty ----
    } else if (op == "setProperty") {
        std::string esp        = JsonGetString(req, "esp");
        std::string formId     = JsonGetString(req, "formId");
        std::string scriptName = JsonGetString(req, "scriptName");
        std::string propName   = JsonGetString(req, "propName");
        double      value      = JsonGetDouble(req, "value");

        F4SE::GetTaskInterface()->AddTask([esp, formId, scriptName, propName, value]() {
            auto* form = LookupFormByPlugin(esp, formId);
            SetPapyrusProperty(form, scriptName, propName, value);
        });

    } else {
        logger::warn("PapyrusBridge: unknown op '{}'", op);
    }

    return JSValueMakeUndefined(ctx);
}

// ---------------------------------------------------------------------------
// Public entry point — called from OnWindowObjectReady
// ---------------------------------------------------------------------------
void InjectBridge(ultralight::View* caller, Core::PrismaViewId viewId) {
    // We are already inside an Ultralight callback on the Ultralight thread.
    // LockJSContext() returns the active scoped lock for this context.
    auto scoped = caller->LockJSContext();
    JSContextRef ctx = scoped->ctx();
    JSObjectRef  global = JSContextGetGlobalObject(ctx);

    // Register __prisma_request as a native C++ function
    JSStringRef funcName = JSStringCreateWithUTF8CString("__prisma_request");
    JSObjectRef funcObj  = JSObjectMakeFunctionWithCallback(ctx, funcName, PrismaRequestCallback);

    // Attach viewId so PrismaRequestCallback can read it without a global map
    JSStringRef vidKey = JSStringCreateWithUTF8CString("viewId");
    JSStringRef vidVal = JSStringCreateWithUTF8CString(std::to_string(viewId).c_str());
    JSObjectSetProperty(ctx, funcObj, vidKey,
                        JSValueMakeString(ctx, vidVal),
                        kJSPropertyAttributeReadOnly, nullptr);
    JSStringRelease(vidKey);
    JSStringRelease(vidVal);

    JSObjectSetProperty(ctx, global, funcName, funcObj, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(funcName);

    // Evaluate the bridge script — creates window.prisma using __prisma_request
    ultralight::String exc;
    caller->EvaluateScript(ultralight::String(kBridgeScript), &exc);
    if (!exc.empty())
        logger::warn("PapyrusBridge [{}]: bridge inject failed: {}", viewId, exc.utf8().data());
    else
        logger::info("PapyrusBridge [{}]: window.prisma injected", viewId);
}

} // namespace PrismaUI::PapyrusBridge
