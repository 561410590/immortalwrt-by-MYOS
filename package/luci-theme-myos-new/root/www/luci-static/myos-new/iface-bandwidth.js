// Entry point on the existing LuCI bandwidth page. The collapsed section performs no sampling.
const container = document.getElementById('interface-monitor');
const message = container.querySelector('.ifmon-load-message');
const host = container.querySelector('.ifmon-content');
const interval = 3;
let monitor, baseline, fullAt = 0, timer, revision = 0, opened, requestedDevice;
let rpcMethods;

async function methods() {
    if (rpcMethods) return rpcMethods;
    const rpc = await window.L.require('rpc');
    rpcMethods = {
        status: rpc.declare({object: 'network.device', method: 'status', params: ['name'], reject: true}),
        history: rpc.declare({
            object: 'luci', method: 'getRealtimeStats', params: ['mode', 'device'],
            expect: {result: []}, reject: true
        })
    };
    return rpcMethods;
}

async function getSnapshot(url) {
    const response = await fetch(url, {credentials: 'same-origin', cache: 'no-store'});
    if (!response.ok) throw new Error('HTTP ' + response.status);
    const snapshot = await response.json();
    if (!snapshot || !Number.isFinite(snapshot.sample) || !snapshot.boot)
        throw new Error('未取得本机接口采样');
    snapshot.ports = Array.isArray(snapshot.ports) ? snapshot.ports : [];
    snapshot.uplinks = Array.isArray(snapshot.uplinks) ? snapshot.uplinks : [];
    return snapshot;
}

function mergeDevices(oldRows, fresh) {
    const old = new Map(oldRows.map(row => [row.name, row]));
    return fresh.map(row => ({...old.get(row.name), ...row}));
}

function sameInventory(old, next) {
    const keys = new Set(old.map(row => row.name));
    return old.length === next.length && next.every(row => keys.has(row.name));
}

async function refresh(ticket) {
    const active = () => revision === ticket && container.open && !document.hidden;
    if (!active()) return;
    try {
        if (!monitor) {
            const { createMonitor } = await import('./iface-monitor.js?v=3');
            if (!active()) return;
            monitor = createMonitor({
                host, networkUrl: container.dataset.networkUrl, interval,
                getStatus: async name => (await methods()).status(name),
                getHistory: async name => (await methods()).history('interface', name)
            });
        }
        let full = !baseline || Date.now() >= fullAt;
        let current = await getSnapshot(full ? container.dataset.statusUrl : container.dataset.fastUrl);
        if (!active()) return;
        if (!full && (current.needs_full || current.boot !== baseline.boot ||
            !sameInventory(baseline.ports, current.ports) || !sameInventory(baseline.uplinks, current.uplinks))) {
            current = await getSnapshot(container.dataset.statusUrl);
            full = true;
        }
        if (!active()) return;
        if (full) {
            baseline = current;
            fullAt = Date.now() + 30000;
        } else {
            current = {
                ...baseline,
                sample: current.sample,
                ts: current.ts,
                ports: mergeDevices(baseline.ports, current.ports),
                uplinks: mergeDevices(baseline.uplinks, current.uplinks)
            };
        }
        if (!monitor.open || requestedDevice) {
            monitor.show(current, requestedDevice);
            requestedDevice = null;
        } else monitor.update(current);
        message.hidden = true;
    } catch (error) {
        if (!active()) return;
        message.textContent = '接口数据读取失败，正在重试。' + (error.message || '');
        message.hidden = false;
    } finally {
        if (active()) timer = setTimeout(() => refresh(ticket), interval * 1000);
    }
}

function reconcile() {
    const enabled = container.open && !document.hidden;
    if (enabled === opened) return;
    opened = enabled;
    clearTimeout(timer);
    const ticket = ++revision;
    if (!enabled) {
        monitor?.hide();
        baseline = null;
        return;
    }
    message.textContent = '正在读取本机接口…';
    message.hidden = false;
    refresh(ticket);
}

function openAnchor() {
    const [anchor, query = ''] = location.hash.split('?');
    if (anchor !== '#interface-monitor') return;
    requestedDevice = new URLSearchParams(query).get('device');
    container.open = true;
    if (opened && requestedDevice) {
        clearTimeout(timer);
        refresh(++revision);
    } else reconcile();
}

container.addEventListener('toggle', reconcile);
document.addEventListener('visibilitychange', reconcile);
window.addEventListener('hashchange', openAnchor);
window.addEventListener('pagehide', () => {
    clearTimeout(timer);
    ++revision;
    opened = false;
    monitor?.hide();
    baseline = null;
});
window.addEventListener('pageshow', reconcile);
openAnchor();
reconcile();
