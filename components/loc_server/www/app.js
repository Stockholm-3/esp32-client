let g_suppress = false;
let g_ws = null;

function connect() {
    g_ws = new WebSocket('ws://' + location.host + '/ws');

    g_ws.onopen = () => {
        document.getElementById('ws-dot').className = 'dot connected';
        document.getElementById('ws-label').textContent = 'Connected';
    };

    g_ws.onclose = () => {
        document.getElementById('ws-dot').className = 'dot disconnected';
        document.getElementById('ws-label').textContent = 'Disconnected — retrying...';
        setTimeout(connect, 3000);
    };

    g_ws.onmessage = (evt) => {
        const msg = JSON.parse(evt.data);
        if (msg.type === 'settings') {
            g_suppress = true;
            document.getElementById('inp-location').value    = msg.location  || '';
            document.getElementById('sel-price').selectedIndex   = msg.price_zone ?? 0;
            document.getElementById('sel-timeout').selectedIndex = msg.timeout    ?? 0;

            const lwc = msg.local_web_client_enabled ?? false;
            document.getElementById('chk-lwc').checked = lwc;
            document.getElementById('ip-config-block').style.display = lwc ? 'block' : 'none';
            document.getElementById('inp-ip').value   = msg.sta_static_ip  || '';
            document.getElementById('inp-gw').value   = msg.sta_gateway    || '';
            document.getElementById('inp-nm').value   = msg.sta_netmask    || '';
            document.getElementById('inp-host').value = msg.mdns_hostname  || '';
            g_suppress = false;
        } else if (msg.type === 'wifi_status') {
            const labels = { connected: 'Connected', connecting: 'Connecting...', failed: 'Failed' };
            document.getElementById('wifi-state-lbl').textContent = labels[msg.state] || msg.state;
        } else if (msg.type === 'wifi_scan_result') {
            const sel = document.getElementById('sel-ssid');
            sel.innerHTML = '<option value="">-- select network --</option>';
            msg.aps.forEach(ap => {
                const opt = document.createElement('option');
                opt.value = ap.ssid;
                opt.textContent = `${ap.ssid}  (${ap.rssi} dBm)${ap.open ? ' 🔓' : ''}`;
                sel.appendChild(opt);
            });
        } else if (msg.type === 'restarting') {
            document.getElementById('reboot-overlay').style.display = 'flex';
        }
    };
}

function scanWifi() {
    if (!g_ws || g_ws.readyState !== WebSocket.OPEN) return;
    g_ws.send(JSON.stringify({ type: 'scan_wifi' }));
}

function sendSettings() {
    if (g_suppress || !g_ws || g_ws.readyState !== WebSocket.OPEN) return;
    g_ws.send(JSON.stringify({
        type:       'set_settings',
        location:   document.getElementById('inp-location').value,
        price_zone: document.getElementById('sel-price').selectedIndex,
        timeout:    document.getElementById('sel-timeout').selectedIndex,
    }));
}

function applyWifi() {
    if (!g_ws || g_ws.readyState !== WebSocket.OPEN) return;
    const ssid = document.getElementById('sel-ssid').value;
    const pass = document.getElementById('inp-pass').value;
    if (!ssid || !pass) { alert('Select network and enter password'); return; }
    g_ws.send(JSON.stringify({ type: 'set_settings', ssid, password: pass }));
}

function sendNetworkConfig() {
    if (g_suppress) return;
    const enabled = document.getElementById('chk-lwc').checked;
    document.getElementById('ip-config-block').style.display = enabled ? 'block' : 'none';
    if (!enabled && g_ws && g_ws.readyState === WebSocket.OPEN) {
        g_ws.send(JSON.stringify({ type: 'set_settings', local_web_client_enabled: false }));
    }
}

function saveNetworkConfig() {
    if (!g_ws || g_ws.readyState !== WebSocket.OPEN) return;
    const ip   = document.getElementById('inp-ip').value.trim();
    const gw   = document.getElementById('inp-gw').value.trim();
    const nm   = document.getElementById('inp-nm').value.trim();
    const host = document.getElementById('inp-host').value.trim();
    if (!ip || !gw || !nm) { alert('IP, Gateway and Netmask are required'); return; }
    g_ws.send(JSON.stringify({
        type: 'set_settings',
        local_web_client_enabled: true,
        sta_static_ip:  ip,
        sta_gateway:    gw,
        sta_netmask:    nm,
        mdns_hostname:  host || 'esp32-client',
    }));
}

connect();
