import {
    bitsText, bytesText, countText, counterSnapshot, defaultRoutes, deviceRows, distinct,
    ipText, linkText, numberOf, orderedRows, speedText, timeText
} from './iface-samples.js?v=3';
import { chartSection } from './iface-trend.js?v=3';

function el(tag, className, text) {
    const node = document.createElement(tag);
    if (className) node.className = className;
    if (text != null) node.textContent = text;
    return node;
}

function setText(node, value) {
    if (node.textContent !== value) node.textContent = value;
}

function button(text, className) {
    const control = el('button', className || 'm-copy-button', text);
    control.type = 'button';
    return control;
}

function linkToNetwork(name, url) {
    const a = el('a', 'ifmon-net-link', name);
    a.href = url + '?iface=' + encodeURIComponent(name);
    a.setAttribute('aria-label', '查看逻辑接口 ' + name);
    return a;
}

function labelLine(title, content) {
    const row = el('div', 'ifmon-field');
    row.append(el('dt', '', title));
    const value = el('dd');
    if (content instanceof Node) value.append(content);
    else value.textContent = content || '—';
    row.append(value);
    return row;
}

function section(title, content) {
    const box = el('section', 'ifmon-section');
    box.append(el('h3', '', title), content);
    return box;
}

function rateText(bytesPerSecond) {
    const bytes = numberOf(bytesPerSecond);
    return bytes === null ? '—' : bitsText(bytes * 8);
}

function stateText(row) {
    if (typeof row.carrier === 'boolean')
        return linkText(row.carrier);
    if (row.nets.some(net => net.up)) return '接口已启动';
    if (row.nets.some(net => net.pending)) return '启动中';
    if (row.nets.length && row.nets.every(net => net.up === false)) return '接口未启动';
    return '未获取';
}

function roleText(row) {
    return distinct([row.role === '出口' && row.isUplink ? '' : row.role, row.isUplink ? '当前出口' : '']).join(' · ');
}

function rowId(row) {
    return distinct([row.label, row.name, row.outlet]).join(' · ');
}

function infoCell(row, index, text) {
    setText(row.cells[index], text);
}

function listView(onSelect) {
    const root = el('div', 'ifmon-overview');
    const tools = el('div', 'ifmon-tools');
    const filter = el('select', 'cbi-input-select');
    filter.setAttribute('aria-label', '接口范围');
    filter.add(new Option('全部接口', 'all'));
    filter.add(new Option('物理网口', 'ports'));
    filter.add(new Option('当前出口', 'uplinks'));
    filter.add(new Option('已关联逻辑网络', 'networks'));
    const search = el('input', 'cbi-input-text');
    search.type = 'search';
    search.placeholder = '搜索接口、网络或 IP';
    search.setAttribute('aria-label', '搜索接口、逻辑网络或 IP 地址');
    const count = el('span', 'ifmon-row-count');
    tools.append(filter, search, count);

    const scroll = el('div', 'ifmon-table-scroll');
    const table = el('table', 'ifmon-table');
    const header = table.createTHead().insertRow();
    const body = table.createTBody();
    const headings = [
        ['name', '接口'], ['', '状态 / 网络'], ['', '本机 IP 地址'],
        ['rxRate', '接收速率'], ['txRate', '发送速率'],
        ['rx', '累计接收'], ['tx', '累计发送']
    ];
    const sortColumns = new Map();
    let latestRows = [], sort = '', ascending = true, visible = true;
    const nodes = new Map();

    for (const [key, title] of headings) {
        const th = el('th');
        th.scope = 'col';
        if (key) {
            const trigger = button(title, 'ifmon-column');
            trigger.addEventListener('click', () => {
                if (sort === key)
                    ascending = !ascending;
                else {
                    sort = key;
                    ascending = key === 'name';
                }
                paint();
            });
            th.append(trigger);
            sortColumns.set(key, th);
        } else th.textContent = title;
        header.append(th);
    }

    const empty = el('p', 'ifmon-note');
    const clear = button('清除筛选');
    const footer = el('div', 'ifmon-list-footer');
    clear.hidden = true;
    footer.append(empty, clear);
    scroll.append(table);
    root.append(tools, scroll, footer);

    function build(device) {
        const tr = el('tr');
        const id = tr.insertCell();
        const pick = button('', 'ifmon-device');
        pick.addEventListener('click', () => onSelect(device));
        id.append(pick, el('small'));
        const state = tr.insertCell();
        state.append(el('span', 'ifmon-state'), el('small'));
        ['本机 IP 地址', '接收速率', '发送速率', '累计接收', '累计发送'].forEach(label => {
            const td = tr.insertCell();
            td.dataset.label = label;
        });
        const address = tr.cells[2];
        address.className = 'ifmon-address';
        const more = button('', 'ifmon-address-more');
        more.addEventListener('click', () => onSelect(device));
        address.append(el('span', 'ifmon-address-first'), more);
        return tr;
    }

    function updateRow(tr, row) {
        const [id, state] = tr.cells;
        setText(id.firstChild, row.label);
        const secondary = distinct([
            row.label !== row.name ? row.name : '',
            row.outlet !== row.label && row.outlet !== row.name ? row.outlet : '',
            roleText(row)
        ]).join(' · ');
        setText(id.lastChild, secondary);
        setText(state.firstChild, stateText(row));
        state.firstChild.dataset.link = row.carrier === true ? 'up' : 'other';
        setText(state.lastChild, row.nets.map(net => net.name).join(' / '));
        const addresses = row.address4.concat(row.address6);
        const address = tr.cells[2];
        setText(address.firstChild, addresses[0] || '—');
        address.firstChild.title = addresses[0] || '';
        const more = address.lastChild;
        more.hidden = addresses.length < 2;
        if (!more.hidden) {
            setText(more, '另 ' + (addresses.length - 1) + ' 个地址');
            more.setAttribute('aria-label', row.name + ' 的全部 ' + addresses.length + ' 个地址');
        }
        infoCell(tr, 3, rateText(row.rate?.rx));
        infoCell(tr, 4, rateText(row.rate?.tx));
        infoCell(tr, 5, bytesText(row.rx));
        infoCell(tr, 6, bytesText(row.tx));
    }

    function paint() {
        if (!visible) return;
        const rows = orderedRows(latestRows, filter.value, search.value, sort, ascending);
        for (const [key, th] of sortColumns) {
            if (sort === key) th.setAttribute('aria-sort', ascending ? 'ascending' : 'descending');
            else th.removeAttribute('aria-sort');
        }
        const keep = new Set(rows.map(row => row.name));
        for (const [name, tr] of nodes) {
            if (!keep.has(name)) {
                tr.remove();
                nodes.delete(name);
            }
        }
        rows.forEach((row, index) => {
            let tr = nodes.get(row.name);
            if (!tr) {
                tr = build(row.name);
                nodes.set(row.name, tr);
            }
            updateRow(tr, row);
            if (body.rows[index] !== tr) body.insertBefore(tr, body.rows[index] || null);
        });
        const filtered = !!search.value.trim() || filter.value !== 'all';
        clear.hidden = !filtered;
        empty.hidden = rows.length > 0;
        empty.textContent = latestRows.length ? '没有匹配的接口。' : '尚未读取到本机接口。';
        count.textContent = filtered ? rows.length + ' / ' + latestRows.length + ' 个接口' :
            latestRows.length + ' 个接口';
    }

    clear.addEventListener('click', () => {
        filter.value = 'all';
        search.value = '';
        paint();
        search.focus();
    });
    search.addEventListener('input', paint);
    filter.addEventListener('change', paint);
    return {
        root,
        show() {visible = true; root.hidden = false; paint();},
        hide() {visible = false; root.hidden = true;},
        update(rows) {
            const usb = rows.some(row => row.isUsb);
            const option = Array.from(filter.options).find(item => item.value === 'usb');
            if (usb && !option) filter.add(new Option('USB 网络设备', 'usb'));
            if (!usb && option) {
                if (filter.value === 'usb') filter.value = 'all';
                option.remove();
            }
            latestRows = rows;
            paint();
        },
        focus(name) { (nodes.get(name)?.cells[0].firstChild || search).focus({preventScroll: true}); }
    };
}

function logicalNetworks(row, networkUrl) {
    const root = el('div', 'ifmon-nets');
    if (!row.nets.length) {
        root.append(el('p', 'ifmon-note', '该设备没有直接绑定的逻辑接口。'));
        return root;
    }
    for (const net of row.nets) {
        const block = el('article', 'ifmon-net');
        const title = el('div', 'ifmon-net-title');
        title.append(linkToNetwork(net.name, networkUrl));
        const state = net.up ? '已启动' : net.pending ? '启动中' : '未启动';
        title.append(el('small', '', [net.proto, state].filter(Boolean).join(' · ')));
        const fields = el('dl', 'ifmon-fields');
        const routes = defaultRoutes(net);
        function gateways(family) {
            return distinct(routes.filter(route => route.target === family).map(route =>
                route.nexthop && !['0.0.0.0','::'].includes(route.nexthop) ? route.nexthop : '直连'
            ));
        }
        const facts = [
            ['IPv4 地址', ipText(net.ipv4)],
            ['IPv6 地址', ipText(net.ipv6)],
            ['IPv6 前缀', ipText(net.prefixes)],
            ['IPv4 默认网关', gateways('0.0.0.0')],
            ['IPv6 默认网关', gateways('::')],
            ['DNS', distinct(Array.isArray(net.dns) ? net.dns : [])]
        ];
        for (const [label, values] of facts) {
            if (!values.length) continue;
            const lines = el('span', 'ifmon-address-lines');
            for (const address of values) lines.append(el('span', '', address));
            fields.append(labelLine(label, lines));
        }
        block.append(title);
        if (fields.childElementCount) block.append(fields);
        root.append(block);
    }
    return root;
}

function deviceDetails(networkUrl) {
    const root = el('div', 'ifmon-detail');
    const heading = el('div', 'ifmon-detail-head');
    const identity = el('h3');
    const role = el('span', 'ifmon-note');
    heading.append(identity, role);
    const chart = chartSection();
    const facts = el('dl', 'ifmon-fields');
    const networkBody = el('div');
    const traffic = el('table', 'ifmon-counters');
    const head = traffic.createTHead().insertRow();
    for (const label of ['计数器', '接收 RX', '发送 TX']) {
        const th = el('th', '', label);
        th.scope = 'col';
        head.append(th);
    }
    const body = traffic.createTBody();
    const counters = [
        ['累计流量', 'bytes', bytesText],
        ['数据包', 'packets', countText],
        ['丢弃包', 'dropped', countText],
        ['错误包', 'errors', countText]
    ].map(([label, key, format]) => {
        const line = body.insertRow();
        const title = el('th', '', label);
        title.scope = 'row';
        line.append(title);
        return {key, format, rx: line.insertCell(), tx: line.insertCell()};
    });
    const deviceMessage = el('p', 'ifmon-note');
    const countersBody = el('div');
    countersBody.append(traffic, deviceMessage);
    root.append(heading, chart.root, section('设备与链路', facts),
        section('收发计数', countersBody), section('逻辑接口', networkBody));
    let row, device = {}, notice = '', deviceKey, networkKey;

    function paint() {
        if (!row) return;
        setText(identity, rowId(row));
        setText(role, roleText(row));
        const currentCarrier = typeof device.carrier === 'boolean' ? device.carrier : row.carrier;
        const fields = [
            ['实际设备', row.name],
            ['USB 类型', row.usbKind],
            ['链路状态', typeof currentCarrier === 'boolean' ? linkText(currentCarrier) : stateText(row)],
            ['协商速率', currentCarrier === false ? '' : speedText(device.speed) || row.speed],
            ['MAC', device.macaddr || row.mac],
            ['MTU', device.mtu ?? row.mtu],
            ['IPv6 MTU', device.mtu6],
            ['设备类型', device.devtype || device.type],
            ['桥成员', Array.isArray(device['bridge-members']) ? device['bridge-members'].join(' · ') : ''],
            ['自动协商', device.autoneg === true ? '已启用' : device.autoneg === false ? '已关闭' : '']
        ].filter(([, value]) => value !== undefined && value !== null && value !== '');
        const key = JSON.stringify(fields);
        if (key !== deviceKey) {
            facts.replaceChildren(...fields.map(([label, value]) => labelLine(label, String(value))));
            deviceKey = key;
        }
        const nets = JSON.stringify(row.nets);
        if (nets !== networkKey) {
            const active = networkBody.contains(document.activeElement) ? document.activeElement.href : '';
            networkBody.replaceChildren(logicalNetworks(row, networkUrl));
            if (active)
                Array.from(networkBody.querySelectorAll('a')).find(link => link.href === active)?.focus({preventScroll: true});
            networkKey = nets;
        }
        const stats = device.statistics || {};
        for (const counter of counters) {
            const rx = stats['rx_' + counter.key] ?? (counter.key === 'bytes' ? row.rx : null);
            const tx = stats['tx_' + counter.key] ?? (counter.key === 'bytes' ? row.tx : null);
            setText(counter.rx, counter.format(rx));
            setText(counter.tx, counter.format(tx));
        }
        setText(deviceMessage, notice);
        deviceMessage.hidden = !notice;
    }

    return {
        root, chart,
        reset(next) {
            row = next;
            deviceKey = networkKey = null;
            device = {};
            notice = '';
            paint();
        },
        update(next) {row = next; paint();},
        status(value, message) {device = value || {}; notice = message || ''; paint();}
    };
}

export function createMonitor(options) {
    const {host, networkUrl, getStatus, getHistory} = options;
    const body = el('div', 'ifmon-panel');
    let active = false;
    const breadcrumb = el('div', 'ifmon-navigation');
    const back = button('返回接口列表');
    const choose = el('select', 'cbi-input-select');
    choose.setAttribute('aria-label', '切换实际设备');
    breadcrumb.append(back, choose);
    const overview = listView(select);
    const detail = deviceDetails(networkUrl);
    const message = el('p', 'ifmon-note');
    message.setAttribute('role', 'status');
    body.append(message, breadcrumb, overview.root, detail.root);
    host.append(body);

    let rows = [], lastCounts = null, boot, lastWall = null;
    let selected = '', generation = 0, timeout;
    let deviceOptions = '';
    const observations = new Map();
    let wallOffset, nativeHistory = [], sampleError = '', fallback = false;
    let statusDenied = false, historyDenied = false;
    const permissionError = result => result.status === 'rejected' &&
        /access denied|permission denied|权限|拒绝访问/i.test(String(result.reason?.message || result.reason));

    function remember(snapshot) {
        if (!Number.isFinite(snapshot.ts) || !Number.isFinite(snapshot.sample)) return;
        const offset = snapshot.ts - snapshot.sample;
        if (wallOffset == null || Math.abs(offset - wallOffset) > 3) {
            observations.clear();
            wallOffset = offset;
        }
        const time = wallOffset + snapshot.sample;
        const present = new Set(rows.map(row => row.name));
        for (const name of observations.keys()) {
            if (!present.has(name)) observations.delete(name);
        }
        for (const row of rows) {
            if (row.rx == null && row.tx == null) continue;
            let samples = observations.get(row.name);
            if (!samples) observations.set(row.name, samples = []);
            if (time > (samples.at(-1)?.[0] ?? 0))
                samples.push([time, row.rx, null, row.tx, null]);
            while (samples.length && samples[0][0] < time - 185) samples.shift();
        }
    }

    function drawAvailableHistory() {
        const local = observations.get(selected) || [];
        const latest = nativeHistory.at(-1)?.[0];
        const hasRecentNative = Number.isFinite(latest) && latest >= (lastWall ?? latest) - 8;
        fallback = !hasRecentNative && local.length >= 2;
        if (fallback) {
            detail.chart.update(local, lastWall, '按本机接口字节计数采样。', Math.max(5, options.interval * 3));
        } else detail.chart.update(nativeHistory, lastWall, sampleError);
    }
    function invalidate() {
        ++generation;
        clearTimeout(timeout);
    }
    function list(messageText = '', focus = false) {
        const previous = selected;
        invalidate();
        selected = '';
        detail.chart.stop();
        detail.root.hidden = true;
        breadcrumb.hidden = true;
        message.textContent = messageText;
        message.hidden = !messageText;
        overview.show();
        if (focus) overview.focus(previous);
    }
    function select(name) {
        const row = rows.find(item => item.name === name);
        if (!row || !active) return;
        invalidate();
        const revision = generation;
        detail.chart.stop();
        selected = name;
        overview.hide();
        breadcrumb.hidden = detail.root.hidden = false;
        message.hidden = true;
        choose.value = name;
        detail.reset(row);
        nativeHistory = [];
        sampleError = '';
        fallback = false;
        detail.chart.start();
        drawAvailableHistory();

        choose.focus({preventScroll: true});
        if (!document.hidden) read(name, revision);
    }
    async function read(name, revision) {
        if (generation !== revision || !active || document.hidden || !host.isConnected) return;
        const results = await Promise.allSettled([
            statusDenied ? null : getStatus(name),
            historyDenied ? null : getHistory(name)
        ]);
        if (generation !== revision || !active || document.hidden) return;
        const [status, history] = results;
        const exists = status.status === 'fulfilled' && status.value && Object.keys(status.value).length > 0;
        statusDenied ||= permissionError(status);
        historyDenied ||= permissionError(history);
        detail.status(exists ? status.value : null, exists ? '' : '仅显示本机首页可读取的计数。');
        nativeHistory = history.status === 'fulfilled' && Array.isArray(history.value) ? history.value : [];
        sampleError = history.status === 'rejected' || historyDenied ? '等待首页连续采样。' : '';
        drawAvailableHistory();
        if (!statusDenied || !historyDenied)
            timeout = setTimeout(() => read(name, revision), 3000);
    }
    function visibility() {
        if (!active || !selected) return;
        invalidate();
        if (!document.hidden) read(selected, generation);
    }
    function endSession() {
        invalidate();
        detail.chart.stop();
        document.removeEventListener('visibilitychange', visibility);
        active = false;
        lastCounts = null;
    }
    back.addEventListener('click', () => list('', true));
    choose.addEventListener('change', () => select(choose.value));

    function update(snapshot, rates, uplinks) {
        if (!snapshot) return;
        const changed = boot && snapshot.boot && boot !== snapshot.boot;
        if (changed) {
            lastCounts = null;
            observations.clear();
            wallOffset = null;
        }
        rows = deviceRows(snapshot, rates, uplinks, lastCounts);
        lastWall = snapshot.ts;
        remember(snapshot);
        boot = snapshot.boot;
        lastCounts = counterSnapshot(snapshot, rows);
        const names = rows.map(row => [row.name, rowId(row)]);
        const key = JSON.stringify(names);
        if (key !== deviceOptions) {
            choose.replaceChildren(...names.map(([name, label]) => new Option(label, name)));
            choose.value = selected;
            deviceOptions = key;
        }
        overview.update(rows);
        if (selected) {
            const current = rows.find(row => row.name === selected);
            if (!current || changed)
                list(changed ? '设备已重新启动，请重新选择接口。' : '所选接口已从本机接口列表移除。', true);
            else {
                detail.update(current);
                if (fallback || !nativeHistory.length) drawAvailableHistory();
            }
        }
    }

    return {
        get open() { return active; },
        update,
        hide: endSession,
        show(snapshot, deviceName) {
            if (!active) {
                active = true;
                selected = '';
                lastCounts = null;
                observations.clear();
                wallOffset = null;
                historyDenied = statusDenied = false;
                list();
                document.addEventListener('visibilitychange', visibility);

            }
            update(snapshot);
            if (deviceName && rows.some(row => row.name === deviceName)) select(deviceName);
            else if (deviceName) list('本机当前接口中没有 ' + deviceName + '。');

        }
    };
}
