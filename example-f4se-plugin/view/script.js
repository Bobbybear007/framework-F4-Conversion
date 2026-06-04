function tab(n) {
    ['p','b','l'].forEach(function(x) {
        var el = document.getElementById('t'+x);
        if (el) el.classList.remove('active');
        var el2 = document.getElementById('tb'+x.toUpperCase());
        if (el2) el2.classList.remove('active');
    });
    var activeTab = document.getElementById('t'+n);
    if (activeTab) activeTab.classList.add('active');
    var activeBtn = document.getElementById('tb'+n.toUpperCase());
    if (activeBtn) activeBtn.classList.add('active');
}

var logEl = document.getElementById('log');
if (!logEl) {
    console.error('[Example Plugin] ERROR: log element not found in HTML — logging disabled');
    logEl = document.createElement('div');  // Fallback to prevent crashes
}
var logEmpty = true;

// Fallback if BindUIEvent hasn't registered the handler yet
// C++ BindUIEvent will overwrite this once the handler is ready
window.sendDataToF4SE = window.sendDataToF4SE || function(data) {
    console.error('[Example Plugin] sendDataToF4SE not yet registered — please try again');
};

// Fallback if BindUIEvent hasn't registered requestClose yet
window.requestClose = window.requestClose || function() {
    console.warn('[Example Plugin] requestClose not yet registered — please try again');
};

function lg(cls, label, msg) {
    if (logEmpty) { logEl.innerHTML = ''; logEmpty = false; }
    var t = new Date().toTimeString().slice(0,8);
    var e = document.createElement('div');
    e.className = 'log-entry';
    // Use textContent to prevent XSS; build DOM safely
    var lt = document.createElement('span'); lt.className = 'lt'; lt.textContent = t;
    var ld = document.createElement('span'); ld.className = 'ld '+cls; ld.textContent = label;
    var lm = document.createElement('span'); lm.className = 'lm'; lm.textContent = msg;
    e.appendChild(lt);
    e.appendChild(ld);
    e.appendChild(lm);
    logEl.appendChild(e);
    if (logEl.children.length > 30) logEl.removeChild(logEl.children[0]);
}
function rv(id, s, t) {
    var e=document.getElementById(id);
    if (!e) { console.error('[Example Plugin] rv(): element '+id+' not found'); return; }
    e.className='rv '+s;
    e.textContent=t;
}

function doGetGlobal() {
    var gbEspEl=document.getElementById('gbEsp'), gbFidEl=document.getElementById('gbFid');
    if (!gbEspEl || !gbFidEl) { console.error('[Example Plugin] doGetGlobal: missing input elements'); return; }
    var esp=gbEspEl.value.trim(), fid=gbFidEl.value.trim();
    if (!esp||!fid) { rv('gbRes','error','enter esp and formId'); return; }
    if (!window.prisma) { rv('gbRes','error','window.prisma not available'); return; }
    rv('gbRes','waiting','…');
    lg('po','prisma→','getGlobal("'+esp+'","'+fid+'")');
    window.prisma.getGlobal(esp,fid).then(function(v) {
        if (v===null||v===undefined) { rv('gbRes','error','null — plugin absent or form not found'); lg('pi','←prisma','null'); }
        else { rv('gbRes','ok',String(v)); lg('pi','←prisma',''+v); }
    });
}
function doSetGlobal() {
    var gbEspEl=document.getElementById('gbEsp'), gbFidEl=document.getElementById('gbFid'), gbValEl=document.getElementById('gbVal');
    if (!gbEspEl || !gbFidEl || !gbValEl) { console.error('[Example Plugin] doSetGlobal: missing input elements'); return; }
    var esp=gbEspEl.value.trim(), fid=gbFidEl.value.trim();
    var raw=gbValEl.value.trim(), v=parseFloat(raw);
    if (!esp||!fid||raw===''||isNaN(v)) { lg('po','prisma→','setGlobal: missing/invalid fields'); return; }
    if (!window.prisma) { lg('po','prisma→','window.prisma not available'); return; }
    lg('po','prisma→','setGlobal("'+esp+'","'+fid+'",'+v+')');
    window.prisma.setGlobal(esp,fid,v);
}
function doGetProp() {
    var gpEspEl=document.getElementById('gpEsp'), gpFidEl=document.getElementById('gpFid'), gpScriptEl=document.getElementById('gpScript'), gpPropEl=document.getElementById('gpProp');
    if (!gpEspEl || !gpFidEl || !gpScriptEl || !gpPropEl) { console.error('[Example Plugin] doGetProp: missing input elements'); return; }
    var esp=gpEspEl.value.trim(), fid=gpFidEl.value.trim();
    var scr=gpScriptEl.value.trim(), prp=gpPropEl.value.trim();
    if (!esp||!fid||!scr||!prp) { rv('gpRes','error','fill in all four fields'); return; }
    if (!window.prisma) { rv('gpRes','error','window.prisma not available'); return; }
    rv('gpRes','waiting','…');
    lg('po','prisma→','getProperty("'+esp+'","'+fid+'","'+scr+'","'+prp+'")');
    window.prisma.getProperty(esp,fid,scr,prp).then(function(v) {
        if (v===null||v===undefined) { rv('gpRes','error','null — form/script/prop not found or VM not ready'); lg('pi','←prisma','null'); }
        else { rv('gpRes','ok',String(v)); lg('pi','←prisma',''+v); }
    });
}
function doSetProp() {
    var gpEspEl=document.getElementById('gpEsp'), gpFidEl=document.getElementById('gpFid'), gpScriptEl=document.getElementById('gpScript'), gpPropEl=document.getElementById('gpProp'), gpValEl=document.getElementById('gpVal');
    if (!gpEspEl || !gpFidEl || !gpScriptEl || !gpPropEl || !gpValEl) { console.error('[Example Plugin] doSetProp: missing input elements'); return; }
    var esp=gpEspEl.value.trim(), fid=gpFidEl.value.trim();
    var scr=gpScriptEl.value.trim(), prp=gpPropEl.value.trim();
    var raw=gpValEl.value.trim();
    if (!esp||!fid||!scr||!prp||raw==='') { lg('po','prisma→','setProperty: missing fields'); return; }
    if (!window.prisma) { lg('po','prisma→','window.prisma not available'); return; }
    lg('po','prisma→','setProperty("'+esp+'","'+fid+'","'+scr+'","'+prp+'",'+raw+')');
    window.prisma.setProperty(esp,fid,scr,prp,raw);
}

window.init = function() {
    var paBadge = document.getElementById('paBadge');
    var paText = document.getElementById('paText');
    var hdr = document.getElementById('hdr');
    if (!paBadge || !paText || !hdr) { console.error('[Example Plugin] init(): missing status elements'); return; }

    if (window.prisma) {
        paBadge.className='badge badge-green';
        paBadge.textContent='AVAILABLE';
        paText.textContent='window.prisma injected — ready to use';
        hdr.textContent='prisma ready';
    } else {
        paBadge.className='badge badge-red';
        paBadge.textContent='NOT FOUND';
        paText.textContent='window.prisma missing — check F4SE log';
        hdr.textContent='no prisma';
    }
    lg('cj','C++→JS','init() — DOM ready');
};

window.updateFocusLabel = function(msg) {
    var focusLabel = document.getElementById('focusLabel');
    var focusBadge = document.getElementById('focusBadge');
    if (!focusLabel || !focusBadge) { console.error('[Example Plugin] updateFocusLabel(): missing focus elements'); return; }

    var focused = msg.indexOf('Focused') !== -1;
    focusLabel.textContent = msg;
    focusBadge.textContent = focused ? 'FOCUSED' : 'UNFOCUSED';
    focusBadge.className   = 'badge ' + (focused ? 'badge-green' : 'badge-amber');
    lg('cj','C++→JS','updateFocusLabel("'+msg+'")');
};

window._resolveGlobalWrite = function(success) {
    var gbRes = document.getElementById('gbRes');
    if (!gbRes) return;
    if (success) {
        gbRes.className = 'rv ok';
        gbRes.textContent = 'Write successful';
        lg('pi','←prisma','setGlobal success');
    } else {
        gbRes.className = 'rv error';
        gbRes.textContent = 'Write failed — form not found or not a global';
        lg('pi','←prisma','setGlobal failed');
    }
};

window._resolvePropertyWrite = function(success) {
    var gpRes = document.getElementById('gpRes');
    if (!gpRes) return;
    if (success) {
        gpRes.className = 'rv ok';
        gpRes.textContent = 'Write successful';
        lg('pi','←prisma','setProperty success');
    } else {
        gpRes.className = 'rv error';
        gpRes.textContent = 'Write failed — form/script/prop not found';
        lg('pi','←prisma','setProperty failed');
    }
};

window._prismaResolveGlobal = function(value) {
    var gbRes = document.getElementById('gbRes');
    if (!gbRes) return;
    if (value !== null && value !== undefined && value !== 0) {
        gbRes.className = 'rv ok';
        gbRes.textContent = String(value);
        lg('pi','←prisma','getGlobal success: ' + value);
    } else {
        gbRes.className = 'rv error';
        gbRes.textContent = 'null — plugin absent or form not found';
        lg('pi','←prisma','getGlobal failed');
    }
};

window._prismaResolveProperty = function(value) {
    var gpRes = document.getElementById('gpRes');
    if (!gpRes) return;
    if (value !== null && value !== undefined && value !== 0) {
        gpRes.className = 'rv ok';
        gpRes.textContent = String(value);
        lg('pi','←prisma','getProperty success: ' + value);
    } else {
        gpRes.className = 'rv error';
        gpRes.textContent = 'null — form/script/prop not found or VM not ready';
        lg('pi','←prisma','getProperty failed');
    }
};

function sendMsg() {
    var msgInEl = document.getElementById('msgIn');
    if (!msgInEl) { console.error('[Example Plugin] sendMsg(): msgIn element not found'); return; }
    var val = msgInEl.value.trim();
    if (!val) return;
    var p = JSON.stringify({message:val});
    lg('jc','JS→C++','sendDataToF4SE('+p+')');
    window.sendDataToF4SE(p);
    msgInEl.value = '';
}
function closePanel() { lg('jc','JS→C++','requestClose()'); window.requestClose(); }
function testConsole() {
    console.log('[PrismaUI Example] console.log test — check F4SE log');
    console.warn('[PrismaUI Example] console.warn test');
    lg('jc','JS→C++','console.log + console.warn fired');
}
var _copyButtonState = { text: '', bg: '' };

function copyEventLog() {
    if (!logEl) return;

    var text = '';
    // Get ALL children, not just ones matching CSS selector (in case there are DOM issues)
    for (var i = 0; i < logEl.children.length; i++) {
        var entry = logEl.children[i];
        if (entry.className === 'le') {
            // Empty state message
            text += entry.textContent + '\n';
        } else if (entry.className.indexOf('log-entry') !== -1) {
            // Real log entry
            var time = '', label = '', msg = '';
            for (var j = 0; j < entry.children.length; j++) {
                var span = entry.children[j];
                if (span.className === 'lt') time = span.textContent;
                else if (span.className.indexOf('ld') === 0) label = span.textContent;
                else if (span.className === 'lm') msg = span.textContent;
            }
            text += time + ' ' + label + ' ' + msg + '\n';
        }
    }

    // Find the copy button and CAPTURE original state BEFORE changing it
    var copyBtn = document.querySelector('button[onclick="copyEventLog()"]');
    _copyButtonState.text = copyBtn ? copyBtn.textContent : 'Copy Log';
    _copyButtonState.bg = copyBtn ? copyBtn.style.background : '';

    // Visual feedback: change button appearance immediately
    if (copyBtn) {
        copyBtn.textContent = 'Copying…';
        copyBtn.style.background = 'rgba(34,197,94,0.7)';
        copyBtn.style.borderColor = 'rgba(34,197,94,0.9)';
    }

    lg('jc','JS→C++','copyEventLog');

    // Send to C++ for clipboard copy (more reliable than JS clipboard APIs in Ultralight)
    window.sendDataToF4SE(JSON.stringify({cmd: 'copyLog', logText: text}));
}

window._resolveCopyLog = function(success) {
    var copyBtn = document.querySelector('button[onclick="copyEventLog()"]');

    if (success) {
        if (copyBtn) {
            copyBtn.textContent = 'Copied!';
            copyBtn.style.background = 'rgba(34,197,94,0.7)';
        }
        lg('pi','←C++','copyLog success');
    } else {
        if (copyBtn) {
            copyBtn.textContent = 'Copy failed';
            copyBtn.style.background = 'rgba(239,68,68,0.7)';
        }
        lg('pi','←C++','copyLog failed');
    }

    // Reset button after 1.5s using CAPTURED original state
    setTimeout(function() {
        if (copyBtn) {
            copyBtn.textContent = _copyButtonState.text;
            copyBtn.style.background = _copyButtonState.bg;
        }
    }, 1500);
};

window._showDiag = function(msg) {
    var gbRes = document.getElementById('gbRes');
    if (!gbRes) return;
    gbRes.className = 'rv ' + (msg.indexOf('ERROR') !== -1 ? 'error' : 'ok');
    gbRes.textContent = msg;
};

function doDiagnose() {
    var gbEspEl = document.getElementById('gbEsp');
    var gbFidEl = document.getElementById('gbFid');
    if (!gbEspEl || !gbFidEl) return;
    var esp = gbEspEl.value.trim();
    var fid = gbFidEl.value.trim();
    if (!esp || !fid) { alert('Enter Plugin and FormID first'); return; }
    lg('jc','JS→C++','diagnose');
    window.sendDataToF4SE(JSON.stringify({cmd:'diagnose', esp: esp, formId: fid}));
}

var msgInEl = document.getElementById('msgIn');
if (msgInEl) {
    msgInEl.addEventListener('keydown', function(e) {
        if (e.key === 'Enter') sendMsg();
    });
}
