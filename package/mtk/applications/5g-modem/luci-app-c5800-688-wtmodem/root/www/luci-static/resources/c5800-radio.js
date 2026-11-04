(function () {
    'use strict';
    var root = document.querySelector('.cms-radio');
    if (!root) return;
    function el(tag, text) { var n = document.createElement(tag); if (text !== undefined) n.textContent = text; return n; }
    function shown(v) { return v === undefined || v === null || v === '' ? '—' : String(v); }
    var policies = { '0': '锁定模式', '1': '智能模式', '2': '无偏好模式' };
    var policyNotes = {
        '0': '保持指定频段；信号变差或失去连接时也不主动解锁。',
        '1': '原厂定义：锁频失效后允许解锁，并采用设备内置的频段优先级。',
        '2': '原厂定义：不附加场景频段优先级。自选多频段需要模组支持；若模组拒绝，显示原始错误并保留原设置。'
    };
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
        var secondaryPanel = iface === 'cpe1';
        var message = panel.querySelector('.cms-radio-message'), result = panel.querySelector('.cms-radio-result');
        var preview = panel.querySelector('.cms-radio-preview'), cells = panel.querySelector('.cms-radio-cells');
        var raw = panel.querySelector('.cms-radio-raw'), snapshot = null, locks = null, busy = false, loaded = false;
        var radioKeys = ['nr', 'lte'];
        function input(name) { return form.elements.namedItem(name); }
        function pending(value) {
            busy = value;
            Array.prototype.forEach.call(panel.querySelectorAll('button, input, select'), function (n) { n.disabled = value; });
        }
        function values() {
            var data = { iface: iface, token: root.getAttribute('data-token'), layout: 'combined' };
            ['kind', 'policy', 'mode'].forEach(function (key) { data[key] = input(key).value; });
            radioKeys.forEach(function (key) {
                data[key + '_enabled'] = input(key + '_enabled').checked ? '1' : '0';
                ['bands', 'arfcn', 'scs', 'pci'].forEach(function (field) { data[key + '_' + field] = input(key + '_' + field).value.trim(); });
            });
            return data;
        }
        function update() {
            var data = values(), kind = data.kind, native = locks && locks.native, commands = [], strict = false;
            panel.querySelector('.cms-radio-auto').hidden = kind !== '0';
            panel.querySelector('.cms-radio-groups').hidden = !secondaryPanel && kind === '0';
            panel.querySelector('[data-field="policy"]').hidden = !secondaryPanel && kind === '0';
            radioKeys.forEach(function (key) {
                var nr = key === 'nr', group = panel.querySelector('[data-radio-group="' + key + '"]');
                var enabled = kind !== '0' && data[key + '_enabled'] === '1';
                group.querySelector('.cms-radio-group-fields').hidden = !secondaryPanel && !enabled;
                group.querySelector('[data-field="bandchoice"]').hidden = kind !== '3' && !(secondaryPanel && kind === '0');
                group.querySelector('.cms-radio-parameters').hidden = kind !== '1' && kind !== '2';
                Array.prototype.forEach.call(group.querySelectorAll('.cms-radio-band-options input'), function (box) {
                    var displayedBands = data[key + '_bands'];
                    if (secondaryPanel && kind === '3' && !displayedBands && data.policy !== '2') displayedBands = locks && locks.default_bands && locks.default_bands[key] || '';
                    box.checked = (!secondaryPanel || enabled) && displayedBands.split(',').indexOf(box.value) >= 0;
                });
                group.querySelector('[data-field="scs"]').hidden = !nr || native;
                group.querySelector('[data-field="pci"]').hidden = kind !== '2';
                var selected = data[key + '_bands'], effectiveKind = enabled ? kind : '0', mobility = data.policy;
                if (effectiveKind === '3' && !selected) {
                    if (data.policy === '2') effectiveKind = '0';
                    else selected = locks && locks.default_bands && locks.default_bands[key] || '';
                }
                if (effectiveKind === '3' && selected && locks) {
                    var supported = (locks.supported || '').match(new RegExp('(?:^|,)' + key + '-([^,]+)'));
                    if (supported) selected = supported[1].split(':').filter(function (band) { return selected.split(',').indexOf(band) >= 0; }).join(',');
                }
                if (effectiveKind !== '3' || (selected && selected.split(',').length === 1)) mobility = '0';
                if (effectiveKind !== '0' && mobility !== data.policy) strict = true;
                var cmd = 'AT^' + (nr ? 'NR' : 'LTE') + 'FREQLOCK=' + effectiveKind;
                if (effectiveKind !== '0') {
                    cmd += ',' + mobility + ',' + (selected ? selected.split(',').length : '?') + ',"' + selected + '"';
                    if (effectiveKind !== '3') cmd += ',"' + data[key + '_arfcn'] + '"' + (nr ? ',"' + data[key + '_scs'] + '"' : '') + (effectiveKind === '2' ? ',"' + data[key + '_pci'] + '"' : '');
                }
                commands.push(native ? (nr ? '5G NR' : '4G LTE') + '：' + (effectiveKind === '0' ? '解除锁定' : '频段 ' + selected + (effectiveKind !== '3' ? ' / 频点 ' + data[key + '_arfcn'] + (effectiveKind === '2' ? ' / PCI ' + data[key + '_pci'] : '') : '')) : cmd);
                group.querySelector('.cms-radio-group-note').textContent = secondaryPanel && enabled && kind === '3' && !data[key + '_bands'] && data.policy !== '2' ? '当前使用原厂默认频段，已按设备配置回填。修改勾选后为自定义频段。' : !enabled ? (secondaryPanel ? '当前未锁定。选择频段或填写小区参数后，点击应用才会生效。' : '不锁定，由模组自动选择。') : kind === '3' ?
                    '频段可多选。全部不选时，智能／锁定模式使用设备默认频段；无偏好模式解除锁定。' :
                    native ? '每组填写一个频点或小区，SCS 由副设备处理。' :
                    '多组参数用英文逗号分隔，按顺序对应；未上报的频段或 SCS 需手动补齐。';
            });
            panel.querySelector('.cms-radio-policy-note').textContent = !secondaryPanel && kind === '0' ? '' :
                (policyNotes[data.policy] || '') + (strict ? ' 原厂对单频段、锁频点及锁小区实际使用锁定模式。' : '');
            preview.textContent = (native ? '副设备原厂设置 · ' + policies[data.policy] : '命令预览') + '\n' + commands.join('\n');
        }
        function renderBands() {
            radioKeys.forEach(function (key) {
                var bandOptions = panel.querySelector('[data-radio-group="' + key + '"] .cms-radio-band-options');
                while (bandOptions.firstChild) bandOptions.removeChild(bandOptions.firstChild);
                var match = locks && (locks.supported || '').match(new RegExp('(?:^|,)' + key + '-([^,]+)'));
                var bands = match ? match[1].split(':') : [];
                bands.forEach(function (band) {
                    if (!/^\d+$/.test(band)) return;
                    var label = el('label'), box = el('input'); box.type = 'checkbox'; box.value = band;
                    box.addEventListener('input', function () {
                        var selected = [];
                        Array.prototype.forEach.call(bandOptions.querySelectorAll('input:checked'), function (n) { selected.push(n.value); });
                        input(key + '_bands').value = selected.join(',');
                        if (secondaryPanel) {
                            input(key + '_enabled').checked = true;
                            if (input('kind').value === '0') input('kind').value = '3';
                        }
                        update();
                    });
                    label.appendChild(box); label.appendChild(el('span', (key === 'nr' ? 'n' : 'B') + band)); bandOptions.appendChild(label);
                });
                if (!bands.length) bandOptions.appendChild(el('p', '未读到支持频段。'));
            });
        }
        function fillLock() {
            var active = null;
            radioKeys.forEach(function (key) {
                var profile = locks && locks[key];
                input(key + '_enabled').checked = !!profile && String(profile.kind) !== '0';
                if (input(key + '_enabled').checked && !active) active = profile;
                ['bands', 'arfcn', 'scs', 'pci'].forEach(function (field) {
                    input(key + '_' + field).value = profile && profile[field] != null ? (secondaryPanel && field === 'bands' && profile.factory_default ? '' : String(profile[field])) : '';
                });
            });
            input('kind').value = active ? String(active.kind) : '0';
            input('policy').value = String((active || locks.nr || locks.lte || {}).policy || '1');
            input('mode').value = secondaryPanel && ['auto', 'sa_only', 'lte'].indexOf(locks.mode) >= 0 ? locks.mode : 'keep';
            result.textContent = (locks.native ? '已读入副设备原厂配置' : '已读入模组实际锁定状态') + ' · 当前 SIM ' + shown(locks.sim);
            if (locks.nr && locks.lte && locks.nr.kind !== '0' && locks.lte.kind !== '0' &&
                (locks.nr.kind !== locks.lte.kind || locks.nr.policy !== locks.lte.policy)) {
                result.textContent += '。5G、4G 当前设置不同；下次应用将使用表单中的统一驻网方式与策略。';
            }
            renderBands(); update();
        }
        function describeLock(profile, nr) {
            if (!profile) return '未读取';
            var kind = String(profile.kind);
            if (kind === '0') return '自动选网 · 未锁定';
            var bands = String(profile.bands || '').split(',').filter(function (band) { return /^\d+$/.test(band); }).map(function (band) { return (nr ? 'n' : 'B') + band; }).join('、');
            if (kind === '3') return (profile.factory_default ? '原厂默认频段 · ' : '锁频段 · ') + bands + ' · ' + (policies[profile.mobility] || '策略未读取');
            return (kind === '2' ? '锁小区 · ' : '锁频点 · ') + bands + ' / ' + shown(profile.arfcn) + (kind === '2' ? ' / PCI ' + shown(profile.pci) : '') + ' · ' + (policies[profile.mobility] || '策略未读取');
        }
        function chooseCell(c) {
            var key = (secondaryPanel ? /^NR(?:\s|$)/.test(c.rat) : c.rat === 'NR') ? 'nr' : 'lte';
            input('kind').value = '2'; input(key + '_enabled').checked = true;
            input(key + '_bands').value = (c.band || '').replace(/^[nB]/, '');
            input(key + '_arfcn').value = c.arfcn === undefined ? '' : String(c.arfcn);
            input(key + '_scs').value = c.scs === undefined ? '' : String(c.scs);
            input(key + '_pci').value = c.pci === undefined ? '' : String(c.pci);
            update();
            result.textContent = c.band && (c.rat !== 'NR' || c.scs !== undefined || (locks && locks.native)) ? '已填入此小区；检查 5G、4G 设置后点击应用。' : '已填入频点和 PCI；模组未上报此邻区的频段 / SCS，请补齐后应用。';
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
                button.addEventListener('click', function () { if (!busy) chooseCell(c); }); if (!data.native || secondaryPanel) action.appendChild(button); row.appendChild(action);
                table.appendChild(row);
            });
            wrap.appendChild(table); cells.appendChild(wrap);
            if (!neighbours.length) cells.appendChild(el('p', data.message || (data.neighbours_available ? '模组当前未上报邻区。' : '邻区读取失败，可稍后刷新。')));
            panel.querySelector('.cms-radio-cell-count').textContent = items.length ? items.length + ' 个' : '暂无数据';
            raw.textContent = data.raw || '';
        }
        function readLocks(callback) {
            request(root.getAttribute('data-lock-status') + '?iface=' + iface, null, function (err, data) {
                if (!err) {
                    locks = data;
                    form.hidden = false;
                    var nativePanel = panel.querySelector('.cms-radio-native');
                    if (nativePanel) nativePanel.hidden = !data.native;
                    var supported = (data.supported || '').split(',').map(function (part) {
                        var sections = part.split('-'), nr = sections[0] === 'nr';
                        return (nr ? 'NR：' : 'LTE：') + (sections[1] || '').split(':').map(function (band) { return (nr ? 'n' : 'B') + band; }).join('、');
                    }).join('；');
                    panel.querySelector('.cms-radio-supported').textContent = '模组支持：' + (supported || '未获取'); fillLock(); raw.textContent += '\n' + (data.raw || '');
                    panel.querySelector('.cms-radio-active-nr').textContent = describeLock(data.nr, true);
                    panel.querySelector('.cms-radio-active-lte').textContent = describeLock(data.lte, false);
                    var modes = { auto: '自动 · 5G / 4G / 3G', sa_only: '仅 5G SA', lte: '仅 4G LTE', '080302': '自动 · 5G / 4G / 3G', '08': '仅 5G SA', '03': '仅 4G LTE' };
                    panel.querySelector('.cms-radio-active-mode').textContent = modes[String(data.mode).replace(/"/g, '')] || shown(data.mode);
                    if (data.read_error) result.textContent = '原厂实时读取失败，当前显示缓存：' + data.read_error;
                }
                callback(err);
            });
        }
        function refresh() {
            if (busy) return;
            pending(true); message.textContent = '正在在线读取驻留、邻区和锁定状态…';
            request(root.getAttribute('data-snapshot') + '?iface=' + iface + '&_=' + Date.now(), null, function (err, data) {
                var errors = [];
                if (err) errors.push('小区：' + err.message);
                else { snapshot = data; renderCells(data); }
                readLocks(function (error) {
                    if (error) errors.push('锁定状态：' + error.message);
                    pending(false);
                    message.textContent = errors.length ? errors.join('；') : '更新于 ' + new Date(data.updated * 1000).toLocaleTimeString() + ' · 所有数值来自此模组';
                    loaded = !err && !error;
                });
            });
        }
        input('kind').addEventListener('change', function () { if (this.value === '2') panel.querySelector('.cms-radio-cell-section').open = true; });
        if (secondaryPanel) {
            input('kind').addEventListener('change', function () {
                if (this.value === '0') radioKeys.forEach(function (key) { input(key + '_enabled').checked = false; });
            });
            radioKeys.forEach(function (key) {
                input(key + '_enabled').addEventListener('change', function () {
                    if (this.checked && input('kind').value === '0') input('kind').value = '3';
                });
                ['bands', 'arfcn', 'scs', 'pci'].forEach(function (field) {
                    input(key + '_' + field).addEventListener('input', function () { input(key + '_enabled').checked = true; });
                });
            });
        }
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
                input('reselect').checked = false;
                result.textContent = err ? err.message : data.message;
                if (!err) { locks = null; snapshot = null; loaded = false; message.textContent = '请读取驻留与邻区，确认实际驻网结果。'; }
                if (!err && secondaryPanel) {
                    readLocks(function (error) {
                        pending(false);
                        if (error) result.textContent = '设置已提交，当前设置读取失败：' + error.message;
                        else if (!locks.read_error) result.textContent = data.message;
                    });
                } else pending(false);
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
