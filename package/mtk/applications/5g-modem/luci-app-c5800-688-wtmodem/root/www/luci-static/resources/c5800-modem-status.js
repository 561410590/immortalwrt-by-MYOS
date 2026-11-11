(function () {
    'use strict';
    var root = document.getElementById('c5800-modem-status');
    if (!root) return;
    var container = document.getElementById('cms-modules');
    var updated = document.getElementById('cms-updated');
    var refresh = document.getElementById('cms-refresh');
    var pending = false;
    var lastSuccess = null;
    var expanded = {};

    function node(tag, className, text) {
        var el = document.createElement(tag);
        if (className) el.className = className;
        if (text !== undefined) el.textContent = text;
        return el;
    }
    function valid(value) { return value !== undefined && value !== null && value !== '' && value !== '-'; }
    function value(data, key) { return valid(data[key]) ? String(data[key]) : '—'; }
    function rows(items) {
        var dl = node('dl');
        items.forEach(function (item) { dl.appendChild(node('dt', '', item[0])); dl.appendChild(node('dd', '', item[1])); });
        return dl;
    }
    function group(title, items) {
        var div = node('div', 'cms-detail-group');
        div.appendChild(node('h4', '', title)); div.appendChild(rows(items)); return div;
    }
    function tile(label, val, unit, quality) {
        var el = node('div', 'cms-tile');
        el.appendChild(node('span', 'cms-label', label));
        var number = node('div', 'cms-value', val);
        if (val !== '—' && unit) number.appendChild(node('span', 'cms-unit', unit));
        el.appendChild(number);
        if (quality) el.appendChild(node('span', 'cms-quality', quality));
        return el;
    }
    function extensionIcon() {
        var el = node('div', 'cms-extension-icon');
        var svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
        svg.setAttribute('viewBox', '0 0 32 32'); svg.setAttribute('aria-hidden', 'true');
        var path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
        path.setAttribute('d', 'M8 11V5m16 6V5M5 14h22v12H5zM10 20h1m4 0h1m4 0h1M16 14V8m-4 0a6 6 0 0 1 8 0M9 4a11 11 0 0 1 14 0');
        svg.appendChild(path); el.appendChild(svg); return el;
    }
    function bandwidth(raw) {
        var n = Number(raw);
        return valid(raw) && isFinite(n) && n > 0 ? (n / 1000) + ' MHz' : '—';
    }
    function card(source, prefix, secondary) {
        var d = {};
        Object.keys(source).forEach(function (key) {
            if (key.indexOf(prefix) === 0) d[key.slice(prefix.length)] = source[key];
        });
        var broadband = secondary && source.cpe1_role === 'broadband';
        var present = d.present === true;
        var connected = d.connected === true;
        var linked = String(d.carrier) === '1';
        var status = connected ? '已联网' : present ? '未联网' : linked ? '等待识别' : '未接入';
        var article = node('article', 'cms-card');
        var head = node('div', 'cms-card-head');
        var title = node('div');
        title.appendChild(node('p', 'cms-slot', secondary ? (broadband ? '2.5G WAN · port5' : '副模组 · PoE 扩展口') : '主模组 · 内置 5G'));
        title.appendChild(node('h3', '', broadband ? '以太网上行' : present ? value(d, 'conntype') : secondary ? '副 5G 扩展' : '内置 5G'));
        head.appendChild(title);
        head.appendChild(node('span', 'cms-badge' + (connected ? ' is-online' : linked ? ' is-waiting' : ''), status));
        article.appendChild(head);
        if (present && !broadband) {
            var network = node('p', 'cms-network');
            network.appendChild(node('strong', '', value(d, 'cops')));
            network.appendChild(node('span', '', value(d, 'mode')));
            network.appendChild(node('span', '', value(d, 'simsel'))); article.appendChild(network);
            var signals = node('div', 'cms-signal');
            var rsrp = Number(d.rscp);
            var quality = !valid(d.rscp) || !isFinite(rsrp) ? '' : rsrp >= -85 ? '强' : rsrp >= -95 ? '良好' : rsrp >= -105 ? '一般' : '弱';
            var rawBand = valid(d.raw_band) ? String(d.raw_band).replace(/^[nNbB]/, '') : '';
            var band = d.band_summary || (rawBand ? (/NR|5G/i.test(d.mode || '') ? 'n' : 'B') + rawBand : '—');
            if (d.carrier_count > 1) network.appendChild(node('span', 'cms-badge', d.carrier_count + 'CC'));
            signals.appendChild(tile('RSRP', value(d, 'rscp'), 'dBm', quality));
            signals.appendChild(tile('SINR', value(d, 'sinr'), 'dB'));
            signals.appendChild(tile('工作频段', band, '')); article.appendChild(signals);
            var basic = rows([
                ['模组温度', valid(d.temper) ? d.temper + ' °C' : '—'], ['固件版本', value(d, 'firmware')],
                ['IPv4 地址', value(d, 'ipv4')], ['IPv6 地址', value(d, 'ipv6')]
            ]);
            basic.className = 'cms-basic'; article.appendChild(basic);
        } else if (broadband && linked) {
            article.appendChild(rows([['连接类型', '以太网上行'], ['IPv4 地址', value(d, 'ipv4')], ['IPv6 地址', value(d, 'ipv6')]]));
        } else {
            var empty = node('div', 'cms-empty'); empty.appendChild(extensionIcon());
            empty.appendChild(node('strong', '', linked ? '链路已连接，等待模组数据' : secondary ? '未接入扩展设备' : '等待内置模组数据'));
            empty.appendChild(node('p', '', secondary ? '支持通过 PoE 扩展口接入兼容的副 5G 设备，连接后在此显示独立状态。' : '模组状态正在读取，请稍后刷新。'));
            article.appendChild(empty);
        }
        var details = node('details'); var slot = secondary ? 'secondary' : 'primary';
        details.open = !!expanded[slot];
        details.addEventListener('toggle', function () { expanded[slot] = details.open; });
        details.appendChild(node('summary', '', '接口与模组详情'));
        details.appendChild(group('接口', [
            ['网络接口', value(d, 'iface')], ['物理设备', value(d, 'ifdev')], ['协议', value(d, 'proto')],
            ['链路状态', linked ? '已连接' : String(d.carrier) === '0' ? '未连接' : '未知'],
            ['默认路由', String(d.defaultroute) === '1' ? '启用' : String(d.defaultroute) === '0' ? '关闭' : '跟随配置'],
            ['路由 Metric', value(d, 'metric')]
        ]));
        if (present && !broadband) {
            details.appendChild(group('模组与 SIM', [
                ['模组厂商', value(d, 'modem')], ['ODU 标识', value(d, 'odu')], ['当前 SIM', value(d, 'simsel')],
                ['SIM 状态', String(d.pin) === '0' || d.pin === 'READY' ? '已就绪' : value(d, 'pin')], ['IMEI', value(d, 'imei')], ['IMSI', value(d, 'imsi')],
                ['ICCID', value(d, 'iccid')], ['本机号码', value(d, 'phone')]
            ]));
            details.appendChild(group('无线与基站', [
                ['RSRQ', value(d, 'rsrq')], ['RSSI', value(d, 'rssi')], ['PLMN', value(d, 'mcc')],
                ['TAC', value(d, 'tac')], ['Cell ID', value(d, 'cid')], ['PCI', value(d, 'pci')],
                ['下行频点', value(d, 'dl_fcn')], ['上行频点', value(d, 'ul_fcn')],
                ['下行带宽', bandwidth(d.dlbw)], ['上行带宽', bandwidth(d.ulbw)],
                ['载波聚合', value(d, 'zbjh')], ['CQI', value(d, 'qci')]
            ]));
            (Array.isArray(d.carriers) ? d.carriers : []).forEach(function (c) {
                function reported(key, unit) { return valid(c[key]) ? String(c[key]) + (unit || '') : '未上报'; }
                details.appendChild(group(c.label + ' · ' + reported('band'), [
                    ['PCI', reported('pci')], ['Cell ID', reported('cell')],
                    ['下行频点', reported('dl_fcn')], ['上行频点', reported('ul_fcn')],
                    ['下行带宽', reported('down_mhz', ' MHz')], ['上行带宽', reported('up_mhz', ' MHz')],
                    ['RSRP', reported('rsrp', ' dBm')], ['RSRQ', reported('rsrq', ' dB')], ['SINR', reported('sinr', ' dB')]
                ]));
            });
            details.appendChild(group('数据连接', [
                ['APN', value(d, 'apn')], ['IPv6 前缀', value(d, 'ipv6pd')],
                ['签约下行', value(d, 'down')], ['签约上行', value(d, 'up')]
            ]));
        }
        article.appendChild(details); return article;
    }
    function render(data) {
        var fragment = document.createDocumentFragment();
        fragment.appendChild(card(data, '', false)); fragment.appendChild(card(data, 'cpe1_', true));
        while (container.firstChild) container.removeChild(container.firstChild);
        container.appendChild(fragment);
    }
    function fetchStatus() {
        if (pending) return;
        pending = true; refresh.disabled = true;
        var xhr = new XMLHttpRequest();
        xhr.open('GET', root.getAttribute('data-status-url') + '?_=' + Date.now(), true); xhr.timeout = 15000;
        function done(error) {
            pending = false; refresh.disabled = false;
            if (error) updated.textContent = '读取失败' + (lastSuccess ? ' · 上次更新 ' + lastSuccess : '，请重试');
        }
        xhr.onload = function () {
            try {
                if (xhr.status !== 200) throw new Error('status');
                var data = JSON.parse(xhr.responseText);
                if (!data || typeof data !== 'object' || !data.iface) throw new Error('payload');
                render(data); lastSuccess = new Date().toLocaleTimeString();
                updated.textContent = '更新于 ' + lastSuccess; done(false);
            } catch (e) { done(true); }
        };
        xhr.onerror = xhr.ontimeout = function () { done(true); }; xhr.send();
    }
    refresh.addEventListener('click', fetchStatus); fetchStatus();
    window.setInterval(function () { if (!document.hidden) fetchStatus(); }, 30000);
}());
