#pragma once

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
/// Layer 2 — CSP meta injection:
///   Inserts <meta http-equiv="Content-Security-Policy"> as the first child of <head>
///   before DOM scripts parse. connect-src:'none' blocks any browser-level network
///   load that the JS kill misses (CSS @import, preconnect, etc.).
///
/// What this does NOT block:
///   - dynamic import() with http: URLs — WebCore may or may not enforce CSP on these;
///     no JS-level override is possible since 'import' is a keyword, not a property.
///   - Non-JS DLL code making WinSock calls directly — out of scope for this sandbox.
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
// Blocks HTML-attribute-initiated network loads: <script src="http...">,
// <img src="http...">, <link href="http...">, CSS @import url("http://..."), etc.
// 'unsafe-inline' and 'unsafe-eval' are kept for compatibility with mod HTML files
// that use inline <script> blocks and eval-style patterns.
try {
    var meta = document.createElement('meta');
    meta.setAttribute('http-equiv', 'Content-Security-Policy');
    meta.setAttribute('content',
        "default-src 'self' 'unsafe-inline' 'unsafe-eval' file: data: blob:; " +
        "connect-src 'none'; " +
        "script-src 'self' 'unsafe-inline' 'unsafe-eval' file: data:; " +
        "img-src 'self' data: blob: file:; " +
        "style-src 'self' 'unsafe-inline' file: data:; " +
        "font-src 'self' data: file:; " +
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
