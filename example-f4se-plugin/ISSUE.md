# CRITICAL BUG: Copy Log Only Copies First Line

**Status:** OPEN [HIGH]  
**Date Opened:** 2026-06-04  
**Root Cause:** JavaScript copyEventLog() uses browser Clipboard API instead of C++ bridge

## Problem

The "Copy Log" button in the Event Log tab only copies the first line of the event log, not the entire log content.

## Root Cause

**JavaScript side (script.js lines 177-254):**
- `copyEventLog()` function tries to copy using `navigator.clipboard.writeText(logText)` 
- Falls back to `document.execCommand('copy')` with DOM selection
- Neither approach reliably handles multi-line text with newlines in Ultralight

**Expected behavior:**
- Should send the complete `logText` to C++ via `window.sendDataToF4SE()`
- C++ side (main.cpp lines 392-403) already has a `copyLog` handler that:
  - Receives the log text as JSON
  - Calls `CopyToSystemClipboard()` which properly unescapes newlines
  - Returns success/failure callback to JS

**Current broken flow:**
- JS tries to copy directly using browser APIs ❌
- Browser APIs don't handle multi-line content correctly in Ultralight
- Only first line gets copied

**Correct flow (not implemented):**
- JS packs `logText` into JSON: `{cmd: 'copyLog', logText: ...}`
- JS sends to C++: `window.sendDataToF4SE(json)`
- C++ receives, unescapes newlines, copies to Windows clipboard via OpenClipboard/SetClipboardData ✓
- C++ calls back to JS with success/failure
- JS updates button feedback

## Solution

Replace `copyEventLog()` in script.js to use the C++ bridge:

```javascript
function copyEventLog() {
    if (!logText) return;

    var copyBtn = document.querySelector('button[onclick="copyEventLog()"]');
    _copyButtonState.text = copyBtn ? copyBtn.textContent : 'Copy Log';
    _copyButtonState.bg = copyBtn ? copyBtn.style.background : '';

    if (copyBtn) {
        copyBtn.textContent = 'Copying…';
        copyBtn.style.background = 'rgba(34,197,94,0.7)';
        copyBtn.style.borderColor = 'rgba(34,197,94,0.9)';
    }

    console.log('[copyEventLog] Sending to C++ bridge: ' + logText.length + ' bytes');
    lg('jc','JS→C++','copyEventLog - ' + logText.length + ' bytes');

    // Send to C++ bridge — it handles Windows clipboard properly
    var payload = JSON.stringify({cmd: 'copyLog', logText: logText});
    window.sendDataToF4SE(payload);
}
```

Remove the `fallbackCopy()` function entirely (lines 216-254) — no longer needed.

The C++ handler already exists and works correctly (main.cpp lines 392-403).

## Files Affected

- `view/script.js` — copyEventLog() function (lines 177-254)
- Depends on main.cpp sendDataToF4SE handler (lines 336-418)

## Testing

After fix:
1. Click "Copy Log" button
2. Paste into text editor
3. Verify **entire event log** is present, not just first line
4. Verify newlines/formatting preserved
