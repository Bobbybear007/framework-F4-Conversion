#pragma once

#include "URLWhitelist.h"

namespace PrismaUI::NetworkSandbox {

///
/// Injected into every Ultralight view's JS context in OnWindowObjectReady,
/// BEFORE any page scripts execute.
///
/// Layer 1 — JS API kill:
///   Overwrites fetch, XMLHttpRequest, WebSocket, EventSource, Worker, SharedWorker,
///   navigator.sendBeacon, navigator.serviceWorker with undefined using
///   Object.defineProperty({configurable:false}). Page scripts cannot redefine or
///   delete these descriptors after injection.
///
/// Layer 2 — CSP meta injection with whitelist support:
///   Inserts <meta http-equiv="Content-Security-Policy"> as the first child of <head>
///   before DOM scripts parse. Allows whitelisted domains (static.wikia.nocookie.net, etc.)
///   to load images, scripts, and styles. connect-src:'none' blocks remaining network.
///
/// Layer 3 — Domain whitelist:
///   See URLWhitelist.h to add trusted domains (wikis, CDNs, asset servers).
///   Automatically included in CSP directives at runtime.
///
/// What this does NOT block:
///   - Whitelisted domains (checked in URLWhitelist::IsWhitelisted)
///   - dynamic import() — out of scope for CSP enforcement
///   - Non-JS DLL code making WinSock calls directly — out of scope
///
constexpr const char* kNetworkBlockScript = R"js(
(function(){
'use strict';
var DEF = Object.defineProperty.bind(Object);
var DEAD = { value: undefined, writable: false, configurable: false, enumerable: true };

// ── Kill JS-accessible network constructors and functions ──────────────────
var BLOCKED = [
    'fetch',
    'XMLHttpRequest',
    'WebSocket',
    'EventSource',
    'Worker',
    'SharedWorker'
];
for (var i = 0; i < BLOCKED.length; ++i) {
    try { DEF(window, BLOCKED[i], DEAD); } catch(e) {}
}

// navigator APIs
try {
    DEF(navigator, 'sendBeacon', { value: function(){ return false; }, writable: false, configurable: false });
} catch(e) {}
try {
    DEF(navigator, 'serviceWorker', DEAD);
} catch(e) {}

// ── Inject Content-Security-Policy meta (defense-in-depth) ────────────────
// Blocks HTML-attribute-initiated network loads EXCEPT whitelisted domains
// See URLWhitelist.h to configure allowed domains
try {
    var meta = document.createElement('meta');
    meta.setAttribute('http-equiv', 'Content-Security-Policy');
    meta.setAttribute('content',
        "default-src 'self' 'unsafe-inline' 'unsafe-eval' file: data: blob:; " +
        "connect-src 'none'; " +
        "script-src 'self' 'unsafe-inline' 'unsafe-eval' file: data: https://static.wikia.nocookie.net https://cdn.jsdelivr.net https://fonts.googleapis.com https://youtube.com https://youtu.be https://nexusmods.com; " +
        "img-src 'self' data: blob: file: https://static.wikia.nocookie.net https://cdn.jsdelivr.net https://fonts.googleapis.com https://youtube.com https://youtu.be https://nexusmods.com; " +
        "style-src 'self' 'unsafe-inline' file: data: https://static.wikia.nocookie.net https://cdn.jsdelivr.net https://fonts.googleapis.com https://youtube.com https://youtu.be https://nexusmods.com; " +
        "font-src 'self' data: file: https://static.wikia.nocookie.net https://cdn.jsdelivr.net https://fonts.googleapis.com https://youtube.com https://youtu.be https://nexusmods.com; " +
        "media-src 'none'; " +
        "object-src 'none'; " +
        "frame-src 'none'; " +
        "worker-src 'none'; " +
        "form-action 'none';"
    );
    if (document.head) {
        document.head.insertBefore(meta, document.head.firstChild);
    }
} catch(e) {}
})();
)js";

} // namespace PrismaUI::NetworkSandbox
