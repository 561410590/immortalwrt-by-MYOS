(function () {
    'use strict';
    var root = document.querySelector('.cms-radio');
    if (!root) return;
    function el(tag, text) { var n = document.createElement(tag); if (text !== undefined) n.textContent = text; return n; }
    function shown(v) { return v === undefined || v === null || v === '' ? '—' : String(v); }
    function request(url, data, callback) {
        var xhr = new XMLHttpRequest(); xhr.timeout = 120000;
        xhr.open(data ? 'POST' : 'GET', url, true);
        if (data) xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.onload = function () {
            try {
                var value = JSON.parse(xhr.responseText);
                if (xhr.status !== 200 || value.error) return callback(new Error(value.error || '请求失败'), value);
                callback(null, value);
            } catch (e) { callback(new Error('返回数据无效，请重新登录后读取状态')); }
        };
        xhr.onerror = xhr.ontimeout = function () { callback(new Error('请求超时或连接中断，请读取实际状态；勿重复提交')); };
        var body = data && Object.keys(data).map(function (key) { return encodeURIComponent(key) + '=' + encodeURIComponent(data[key]); }).join('&');
        xhr.send(body || null);
    }
    var controllers = {};
    Array.prototype.forEach.call(root.querySelectorAll('.cms-radio-panel'), function (panel) {
        var iface = panel.getAttribute('data-iface'), form = panel.querySelector('form');
        var message = panel.querySelector('.cms-radio-message'), result = panel.querySelector('.cms-radio-result');
        var preview = panel.querySelector('.cms-radio-preview'), cells = panel.querySelector('.cms-radio-cells');
        var raw = panel.querySelector('.cms-radio-raw'), snapshot = null, locks = null, busy = false, loaded = false;
        var bandOptions = panel.querySelector('.cms-radio-band-options');
        function input(name) { return form.elements.namedItem(name); }
        function pending(value) {
            busy = value;
            Array.prototype.forEach.call(panel.querySelectorAll('button, input, select'), function (n) { n.disabled = value; });
        }
        function values() {
            var data = { iface: iface, token: root.getAttribute('data-token') };
            ['rat', 'kind', 'bands', 'arfcn', 'scs', 'pci', 'mobility', 'mode'].forEach(function (key) { data[key] = input(key).value.trim(); });
            return data;
        }
        function update() {
            var data = values(), kind = data.kind, nr = data.rat === 'NR';
            panel.querySelector('[data-field="bandchoice"]').hidden = kind !== '3';
            panel.querySelector('.cms-radio-auto').hidden = kind !== '0';
            panel.querySelector('.cms-radio-parameters').hidden = kind !== '1' && kind !== '2';
            Array.prototype.forEach.call(bandOptions.querySelectorAll('input'), function (box) { box.checked = data.bands.split(',').indexOf(box.value) >= 0; });
            ['bands', 'arfcn', 'scs', 'pci', 'mobility'].forEach(function (key) {
                var show = kind !== '0' && (key !== 'bands' || kind !== '3') && (key !== 'arfcn' || kind !== '3') && (key !== 'scs' || (nr && kind !== '3')) && (key !== 'pci' || kind === '2');
                panel.querySelector('[data-field="' + key + '"]').hidden = !show;
            });
            var cmd = 'AT^' + (nr ? 'NR' : 'LTE') + 'FREQLOCK=' + kind;
            if (kind !== '0') {
                cmd += ',' + data.mobility + ',' + (data.bands ? data.bands.split(',').length : '?') + ',"' + data.bands + '"';
                if (kind !== '3') cmd += ',"' + data.arfcn + '"' + (nr ? ',"' + data.scs + '"' : '') + (kind === '2' ? ',"' + data.pci + '"' : '');
            }
            preview.textContent = (iface === 'cpe' ? '主模组' : '副模组') + ' · 命令预览\n' + cmd;
        }
        function renderBands() {
            while (bandOptions.firstChild) bandOptions.removeChild(bandOptions.firstChild);
            var nr = input('rat').value === 'NR', key = nr ? 'nr' : 'lte';
            var match = locks && (locks.supported || '').match(new RegExp('(?:^|,)' + key + '-([^,]+)'));
            var bands = match ? match[1].split(':').sort(function (a, b) { return Number(b) - Number(a); }) : [];
            bands.forEach(function (band) {
                if (!/^\d+$/.test(band)) return;
                var label = el('label'), box = el('input'); box.type = 'checkbox'; box.value = band;
                box.addEventListener('change', function () {
                    var selected = [];
                    Array.prototype.forEach.call(bandOptions.querySelectorAll('input:checked'), function (n) { selected.push(n.value); });
                    input('bands').value = selected.join(','); update();
                });
                label.appendChild(box); label.appendChild(el('span', (nr ? 'n' : 'B') + band)); bandOptions.appendChild(label);
            });
            if (!bands.length) bandOptions.appendChild(el('p', '先读取模组支持频段。'));
        }
        function fillLock() {
            var profile = locks && locks[input('rat').value.toLowerCase()];
            if (!profile) { result.textContent = '没有读到此制式的实际锁定状态。'; return; }
            ['kind', 'bands', 'arfcn', 'scs', 'pci', 'mobility'].forEach(function (key) { input(key).value = shown(profile[key]) === '—' ? '' : String(profile[key]); });
            result.textContent = '已读入模组实际锁定状态 · 当前 SIM ' + locks.sim + ' · 网络模式 ' + locks.mode;
            renderBands();
            update();
        }
        function describeLock(profile, nr) {
            if (!profile) return '未读取';
            var kind = String(profile.kind);
            if (kind === '0') return '自动选网 · 未锁定';
            var bands = String(profile.bands || '').split(',').filter(function (band) { return /^\d+$/.test(band); }).map(function (band) { return (nr ? 'n' : 'B') + band; }).join('、');
            if (kind === '3') return '锁频段 · ' + bands;
            return (kind === '2' ? '锁小区 · ' : '锁频点 · ') + bands + ' / ' + shown(profile.arfcn) + (kind === '2' ? ' / PCI ' + shown(profile.pci) : '');
        }
        function chooseCell(c) {
            input('rat').value = c.rat; input('kind').value = '2'; input('bands').value = (c.band || '').replace(/^[nB]/, '');
            input('arfcn').value = c.arfcn === undefined ? '' : String(c.arfcn);
            input('scs').value = c.scs === undefined ? '' : String(c.scs); input('pci').value = c.pci === undefined ? '' : String(c.pci);
            renderBands(); update();
            result.textContent = c.band && (c.rat !== 'NR' || c.scs !== undefined) ? '已选此小区，点击应用后才会下发到模组。' : '已填入频点和 PCI；模组未上报此邻区的频段 / SCS，请补齐后应用。';
            form.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
        }
        function renderCells(data) {
            while (cells.firstChild) cells.removeChild(cells.firstChild);
            var wrap = el('div'); wrap.className = 'cms-radio-scroll';
            var table = el('table'), header = el('tr');
            ['状态', '制式/频段', 'NCI / ECI', 'RSRP', 'SINR', 'SSB ARFCN / EARFCN', 'PCI', 'SCS', 'TAC', '操作'].forEach(function (name) { header.appendChild(el('th', name)); });
            table.appendChild(header);
            var items = [];
            if (data.serving) items.push({ label: '驻留', cell: data.serving });
            var neighbours = Array.isArray(data.neighbours) ? data.neighbours : [];
            neighbours.forEach(function (cell) { items.push({ label: '邻区', cell: cell }); });
            items.forEach(function (item) {
                var c = item.cell, row = el('tr');
                [item.label, c.rat + ' ' + (c.band || ''), shown(c.cell_id), shown(c.rsrp), shown(c.sinr), shown(c.arfcn), shown(c.pci), shown(c.scs), shown(c.tac)].forEach(function (v) { row.appendChild(el('td', v)); });
                var action = el('td'), button = el('button', '选择小区'); button.type = 'button'; button.className = 'cbi-button';
                button.addEventListener('click', function () { if (!busy) chooseCell(c); }); action.appendChild(button); row.appendChild(action);
                table.appendChild(row);
            });
            wrap.appendChild(table); cells.appendChild(wrap);
            if (!neighbours.length) cells.appendChild(el('p', data.neighbours_available ? '模组当前未上报邻区。' : '邻区读取失败，可稍后刷新。'));
            panel.querySelector('.cms-radio-cell-count').textContent = items.length ? items.length + ' 个' : '暂无数据';
            raw.textContent = data.raw || '';
        }
        function readLocks(callback) {
            request(root.getAttribute('data-lock-status') + '?iface=' + iface, null, function (err, data) {
                if (!err) {
                    locks = data;
                    var supported = (data.supported || '').split(',').map(function (part) {
                        var sections = part.split('-'), nr = sections[0] === 'nr';
                        return (nr ? 'NR：' : 'LTE：') + (sections[1] || '').split(':').sort(function (a, b) { return Number(b) - Number(a); }).map(function (band) { return (nr ? 'n' : 'B') + band; }).join('、');
                    }).join('；');
                    panel.querySelector('.cms-radio-supported').textContent = '模组支持：' + (supported || '未获取'); fillLock(); raw.textContent += '\n' + data.raw;
                    panel.querySelector('.cms-radio-active-nr').textContent = describeLock(data.nr, true);
                    panel.querySelector('.cms-radio-active-lte').textContent = describeLock(data.lte, false);
                    var modes = { auto: '自动 · 5G / 4G / 3G', sa_only: '仅 5G SA', lte: '仅 4G LTE', '080302': '自动 · 5G / 4G / 3G', '08': '仅 5G SA', '03': '仅 4G LTE' };
                    panel.querySelector('.cms-radio-active-mode').textContent = modes[String(data.mode).replace(/"/g, '')] || shown(data.mode);
                }
                callback(err);
            });
        }
        function refresh() {
            if (busy) return;
            pending(true); message.textContent = '正在在线读取驻留、邻区和锁定状态…';
            request(root.getAttribute('data-snapshot') + '?iface=' + iface + '&_=' + Date.now(), null, function (err, data) {
                if (err) { message.textContent = err.message; pending(false); return; }
                snapshot = data; renderCells(data);
                readLocks(function (error) {
                    pending(false); message.textContent = error ? error.message : '更新于 ' + new Date(data.updated * 1000).toLocaleTimeString() + ' · 所有数值来自此模组'; loaded = !error;
                });
            });
        }
        input('rat').addEventListener('change', fillLock);
        input('kind').addEventListener('change', function () { if (this.value === '2') panel.querySelector('.cms-radio-cell-section').open = true; });
        form.addEventListener('input', update); form.addEventListener('change', update);
        panel.querySelector('[data-radio-refresh]').addEventListener('click', refresh);
        panel.querySelector('[data-radio-current]').addEventListener('click', function () {
            if (!snapshot || !snapshot.serving) { result.textContent = '请先读取当前驻留小区。'; return; }
            var c = snapshot.serving;
            if (!c.band) { result.textContent = '未取得实际频段，不能自动填入。'; return; }
            chooseCell(c);
        });
        form.addEventListener('submit', function (event) {
            event.preventDefault(); if (busy) return;
            if (!input('reselect').checked) { result.textContent = '请先确认此模组会重新驻网。'; return; }
            pending(true); result.textContent = '正在设置并读回确认，请等待…';
            request(root.getAttribute('data-lock-set'), values(), function (err, data) {
                pending(false); input('reselect').checked = false;
                result.textContent = err ? err.message : data.message;
                if (!err) { locks = null; snapshot = null; loaded = false; message.textContent = '请读取驻留与邻区，确认实际驻网结果。'; }
            });
        });
        controllers[iface] = function () { if (!loaded && !busy) refresh(); };
        update();
    });
    var tabs = Array.prototype.slice.call(root.querySelectorAll('[data-radio-tab]'));
    function activate(tab) {
        var iface = tab.getAttribute('data-radio-tab');
        tabs.forEach(function (item) {
            var selected = item === tab;
            item.setAttribute('aria-selected', String(selected)); item.tabIndex = selected ? 0 : -1;
            document.getElementById(item.getAttribute('aria-controls')).hidden = !selected;
        });
        if (controllers[iface]) controllers[iface]();
    }
    tabs.forEach(function (tab, index) {
        tab.addEventListener('click', function () { activate(tab); });
        tab.addEventListener('keydown', function (event) {
            var next;
            if (event.key === 'ArrowRight') next = (index + 1) % tabs.length;
            else if (event.key === 'ArrowLeft') next = (index + tabs.length - 1) % tabs.length;
            else if (event.key === 'Home') next = 0;
            else if (event.key === 'End') next = tabs.length - 1;
            else return;
            event.preventDefault(); tabs[next].focus(); activate(tabs[next]);
        });
    });
    if (tabs.length) activate(tabs[0]);
}());
