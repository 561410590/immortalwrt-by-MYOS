// MYOS-NEW interface monitor. Names used for queries are always kernel device names.
const list = value => Array.isArray(value) ? value : [];

export function numberOf(value) {
    if (typeof value !== 'number' || !Number.isFinite(value) || value < 0)
        return null;
    return value;
}

const numeral = new Intl.NumberFormat('zh-CN', { maximumFractionDigits: 2 });
const integer = new Intl.NumberFormat('zh-CN', { maximumFractionDigits: 0 });
const clock = new Intl.DateTimeFormat('zh-CN', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
    hourCycle: 'h23'
});

export function timeText(second) {
    return numberOf(second) === null ? '—' : clock.format(second * 1000);
}

function quantity(value, radix, units) {
    if (numberOf(value) === null)
        return '—';
    let unit = 0;
    while (value >= radix && unit < units.length - 1) {
        value /= radix;
        unit++;
    }
    return numeral.format(value) + ' ' + units[unit];
}

export function bytesText(value) {
    return quantity(value, 1024, ['B', 'KiB', 'MiB', 'GiB', 'TiB', 'PiB']);
}

export function bitsText(bits) {
    return quantity(bits, 1000, ['bps', 'Kbps', 'Mbps', 'Gbps', 'Tbps']);
}

export function packetsText(value) {
    return numberOf(value) === null ? '—' : numeral.format(value) + ' 包/秒';
}

export function countText(value) {
    return numberOf(value) === null ? '—' : integer.format(value);
}

export function speedText(value) {
    if (value == null || value === '')
        return '';
    const match = /^(\d+(?:\.\d+)?)([FH]?)$/i.exec(String(value));
    if (!match)
        return String(value);
    if (Number(match[1]) <= 0)
        return '';
    const duplex = { F: '全双工', H: '半双工' }[match[2].toUpperCase()];
    return [bitsText(Number(match[1]) * 1e6), duplex].filter(Boolean).join(' · ');
}

export function linkText(carrier) {
    if (carrier === true) return '已连接';
    if (carrier === false) return '未连接';
    return '未获取';
}

export function ipText(values) {
    return [...new Set(list(values).filter(ip => ip?.address).map(ip =>
        ip.address + (ip.mask == null ? '' : '/' + ip.mask)
    ))];
}

export function distinct(values) {
    return [...new Set(values.filter(value => typeof value === 'string' && value.length))];
}

export function defaultRoutes(net) {
    return list(net?.routes).filter(route =>
        route && route.mask != null && Number(route.mask) === 0 && (route.target === '0.0.0.0' || route.target === '::')
    );
}

// Only the board collector knows which switch ports are on the case.
// Logical network names can repeat across IPv4 and IPv6 on the same device.
export function deviceRows(snapshot, portRates, outletLabels, previous) {
    const ports = new Map(list(snapshot.ports).map(port => [port.name, port]));
    const uplinks = new Map(list(snapshot.uplinks).map(link => [link.name, link]));
    const usb = new Map(list(snapshot.usb_network).map(item => [item.name, item]));
    const usbKinds = {ethernet: 'USB 网卡 / CDC 共享', rndis: '安卓 USB 共享', iphone: 'iPhone USB 共享'};
    const networks = new Map();
    for (const net of list(snapshot.interfaces)) {
        if (!net.device || !net.name || net.name === 'loopback' || net.device === 'lo')
            continue;
        if (!networks.has(net.device))
            networks.set(net.device, []);
        networks.get(net.device).push(net);
    }
    const names = distinct([...ports.keys(), ...uplinks.keys(), ...usb.keys(), ...networks.keys()]);
    const result = [];
    for (const name of names) {
        const port = ports.get(name);
        const uplink = uplinks.get(name);
        const external = usb.get(name);
        const nets = networks.get(name) || [];
        const origin = port || uplink;
        const address4 = ipText(nets.filter(net => net.up).flatMap(net => list(net.ipv4)));
        const address6 = ipText(nets.filter(net => net.up).flatMap(net => list(net.ipv6)));
        const label = port?.label || uplink?.label || name;
        const outlet = uplink && outletLabels?.get(name);
        const rx = numberOf(origin?.rx);
        const tx = numberOf(origin?.tx);
        let rate = portRates?.[name];
        if (!rate && origin && previous && previous.boot === snapshot.boot) {
            const old = previous.counts.get(name);
            const dt = snapshot.sample - previous.at;
            if (dt > 0 && dt <= 12 && old)
                rate = { rx: deltaRate(old.rx, rx, dt), tx: deltaRate(old.tx, tx, dt) };
            else if (dt === 0 && old)
                rate = old.rate;
        }
        result.push({
            name, label, outlet, rx, tx, rate, nets, address4, address6,
            isPort: !!port, isUplink: !!uplink,
            role: port?.role || external?.role || (uplink ? '出口' : ''),
            isUsb: !!external, usbKind: external && (usbKinds[external.kind] || 'USB 网络设备'),
            carrier: origin?.carrier,
            speed: speedText(origin?.speed),
            mac: origin?.mac, mtu: origin?.mtu
        });
    }
    return result;
}

export function counterSnapshot(snapshot, rows) {
    return {
        at: snapshot.sample,
        boot: snapshot.boot,
        counts: new Map(rows.map(row => [row.name, {rx: row.rx, tx: row.tx, rate: row.rate}]))
    };
}

export function deltaRate(first, last, seconds) {
    if (numberOf(first) === null || numberOf(last) === null || last < first || seconds <= 0)
        return null;
    return (last - first) / seconds;
}

// luci.getRealtimeStats("interface") returns [time, RX bytes, RX packets, TX bytes, TX packets].
// Each point describes an interval with real endpoints. Nulls break the path on resets or gaps.
export function seriesFromCounters(rows, windowSeconds, maxGap = 5) {
    const times = [], rxBits = [], txBits = [], rxPackets = [], txPackets = [];
    let last = null;
    const ordered = list(rows).filter(row =>
        Array.isArray(row) && row.length >= 5 && numberOf(row[0]) !== null && row[0] > 0
    );
    const end = ordered.at(-1)?.[0];
    for (const row of ordered) {
        const time = row[0];
        if (last && time <= last[0])
            continue;
        if (last && time >= end - windowSeconds) {
            const span = time - last[0];
            const rate = index => span > maxGap ? null : deltaRate(last[index], row[index], span);
            const rx = rate(1);
            const tx = rate(3);
            times.push(time);
            rxBits.push(rx === null ? null : rx * 8);
            txBits.push(tx === null ? null : tx * 8);
            rxPackets.push(rate(2));
            txPackets.push(rate(4));
        }
        last = row;
    }
    return { times, rxBits, txBits, rxPackets, txPackets };
}

export function latestPoint(series, mode) {
    const receive = mode === 'packets' ? series.rxPackets : series.rxBits;
    const transmit = mode === 'packets' ? series.txPackets : series.txBits;
    return {
        time: series.times.at(-1),
        rx: receive.at(-1),
        tx: transmit.at(-1)
    };
}

export function orderedRows(rows, filter, search, sort, ascending) {
    const terms = search.toLocaleLowerCase().trim().split(/\s+/).filter(Boolean);
    const filtered = rows.filter(row => {
        if (filter === 'ports' && !row.isPort) return false;
        if (filter === 'uplinks' && !row.isUplink) return false;
        if (filter === 'usb' && !row.isUsb) return false;
        if (filter === 'networks' && !row.nets.length) return false;
        if (!terms.length) return true;
        const haystack = [row.name, row.label, row.outlet, row.role, row.mac,
            ...row.nets.map(net => net.name), ...row.address4, ...row.address6
        ].filter(Boolean).join(' ').toLocaleLowerCase();
        return terms.every(term => haystack.includes(term));
    });
    if (!sort) return filtered;
    const value = row => {
        if (sort === 'name') return row.label;
        if (sort === 'rx' || sort === 'tx') return numberOf(row[sort]);
        return numberOf(row.rate?.[sort === 'rxRate' ? 'rx' : 'tx']);
    };
    return filtered.sort((a, b) => {
        const x = value(a), y = value(b);
        if (x == null || y == null)
            return x == null ? (y == null ? 0 : 1) : -1;
        const comparison = typeof x === 'string'
            ? x.localeCompare(y, 'zh-CN', {numeric: true})
            : x - y;
        return (ascending ? 1 : -1) * comparison;
    });
}
