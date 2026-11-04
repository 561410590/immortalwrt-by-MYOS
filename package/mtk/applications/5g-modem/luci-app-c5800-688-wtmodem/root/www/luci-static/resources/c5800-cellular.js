(function () {
    'use strict';
    function init() {
        var marker = document.getElementById('c5800-cellular-marker');
        if (marker) {
            var map = marker.closest('.cbi-map');
            if (map) map.classList.add('c5800-cellular');
        }
        document.querySelectorAll('.cellular-poe').forEach(function (widget) {
            var status = widget.querySelector('.cellular-poe-state');
            var buttons = widget.querySelectorAll('[data-poe]');
            var pending = false;
            var state = null;
            function controls() {
                buttons.forEach(function (button) {
                    var action = button.getAttribute('data-poe');
                    button.disabled = pending || (action !== 'refresh' &&
                        (!state || !state.supported || String(Number(state.enabled)) === action ||
                            (action === '1' && state.secondary_role === 'broadband')));
                });
            }
            function request(action) {
                if (pending) return;
                pending = true; controls();
                status.textContent = action === 'refresh' ? '正在读取状态…' : '正在' + (action === '1' ? '开启' : '关闭') + '供电…';
                var xhr = new XMLHttpRequest();
                xhr.open(action === 'refresh' ? 'GET' : 'POST', widget.getAttribute(action === 'refresh' ? 'data-poe-status' : 'data-poe-set'), true);
                xhr.timeout = 10000;
                function failed(message) { pending = false; state = null; status.textContent = message; controls(); }
                xhr.onload = function () {
                    try {
                        var data = JSON.parse(xhr.responseText);
                        if (xhr.status !== 200 || data.error) throw new Error(data.error || '操作失败，请刷新状态');
                        state = data;
                        status.textContent = !data.supported ? '此设备未检测到 PoE 供电控制器' :
                            data.enabled ? '供电已开启 · ' + (data.carrier ? '网口链路已连接' : '等待副模组网口连接') :
                                (data.secondary_role === 'broadband' ? '普通 WAN 模式 · ' : '') + '供电已关闭';
                        pending = false; controls();
                    } catch (e) { failed(e.message); }
                };
                xhr.onerror = xhr.ontimeout = function () { failed('请求超时或连接失败，请刷新状态后再操作'); };
                if (action === 'refresh') xhr.send();
                else {
                    xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
                    xhr.send('enabled=' + action + '&token=' + encodeURIComponent(widget.getAttribute('data-token')));
                }
            }
            buttons.forEach(function (button) { button.addEventListener('click', function () { request(button.getAttribute('data-poe')); }); });
            request('refresh');
        });
        document.querySelectorAll('.cellular-route').forEach(function (widget) {
            var q = function (s) { return widget.querySelector(s); };
            var all = function (s) { return Array.from(widget.querySelectorAll(s)); };
            var field = function (s) { return q('[data-field="' + s + '"]'); };
            var names = ['main', 'poe', 'wan'], labels = {main: '5G', poe: '副 5G', wan: 'WAN'};
            var titles = {Single: '单线路', Priority: '备份', Dividing: '分流', Overlap: '本地叠加'};
            var status = q('.cellular-route-state'), state, pending = false, rulesDirty = false, dirty = false;
            var mode = 'Priority', priority = ['poe', 'main', 'wan'];
            function busy(value) {
                pending = value;
                all('button, input, select, textarea').forEach(function (el) { el.disabled = value; });
                q('[data-route-save]').disabled = value || !state;
            }
            function api(endpoint, data) {
                return new Promise(function (resolve, reject) {
                    var xhr = new XMLHttpRequest();
                    xhr.open(data === undefined ? 'GET' : 'POST', widget.getAttribute(endpoint), true);
                    xhr.timeout = 30000;
                    xhr.onload = function () {
                        try {
                            var value = JSON.parse(xhr.responseText);
                            if (xhr.status !== 200 || value.error || value.ok === false) throw new Error(value.error || '设置未能应用');
                            resolve(value);
                        } catch (e) { reject(e); }
                    };
                    xhr.onerror = xhr.ontimeout = function () { reject(new Error('请求失败，请刷新状态核对实际设置')); };
                    if (data === undefined) xhr.send();
                    else {
                        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
                        xhr.send('token=' + encodeURIComponent(widget.getAttribute('data-token')) + '&data=' + encodeURIComponent(JSON.stringify(data)));
                    }
                });
            }
            function enabled(name) { var input = q('[data-line="' + name + '"]'); return !!(input && input.checked); }
            function option(select, name) {
                var item = document.createElement('option'); item.value = name; item.textContent = labels[name]; select.appendChild(item);
            }
            function clientOptions(select, selected) {
                select.textContent = '';
                names.filter(enabled).forEach(function (name) { option(select, name); });
                if (selected && !enabled(selected)) {
                    option(select, selected);
                    select.lastChild.disabled = true;
                    select.lastChild.textContent += '（未启用）';
                }
                if (selected) select.value = selected;
            }
            function refreshFields() {
                var activeCount = names.filter(enabled).length, multiple = activeCount > 1;
                if (multiple && mode === 'Single') mode = 'Priority';
                q('[data-multi-label]').hidden = !multiple;
                q('.route-modes').hidden = !multiple;
                q('[data-multi-help]').hidden = !multiple;
                q('[data-single-help]').hidden = activeCount !== 1;
                all('[data-mode]').forEach(function (b) { b.setAttribute('aria-pressed', String(b.getAttribute('data-mode') === mode)); });
                all('[data-panel]').forEach(function (p) { p.hidden = !multiple || p.getAttribute('data-panel') !== mode; });
                q('.route-wan').hidden = !enabled('wan');
                q('[data-port="wan"]').textContent = enabled('wan') ? 'WAN' : 'LAN';
                q('[data-port="poe"]').textContent = enabled('poe') ? labels.poe : labels.poe + '（停用）';
                all('[data-wan-proto]').forEach(function (p) { p.hidden = p.getAttribute('data-wan-proto') !== field('proto').value; });
                q('.route-dns').hidden = field('peerdns').checked;
                names.forEach(function (name) {
                    q('[data-weight-row="' + name + '"]').hidden = !enabled(name);
                    q('[data-address="' + name + '"]').hidden = !enabled(name);
                    if (state) q('[data-online="' + name + '"]').textContent = enabled(name) !== state.lines[name].enabled ?
                        (enabled(name) ? '待启用' : '待停用') : enabled(name) ? (state.lines[name].online ? '在线' : '离线') : '未启用';
                });
                var selected = field('default').value;
                field('default').textContent = '';
                priority.filter(enabled).forEach(function (name) { option(field('default'), name); });
                if (enabled(selected)) field('default').value = selected;
                all('.route-client-row select').forEach(function (select) { clientOptions(select, select.value); });
                all('[data-dividing-panel]').forEach(function (p) { p.hidden = p.getAttribute('data-dividing-panel') !== field('dividing').value; });
                var box = q('.route-priority'); box.textContent = '';
                priority.filter(enabled).forEach(function (name, index, active) {
                    var row = document.createElement('div'), label = document.createElement('span'), button = document.createElement('button');
                    row.className = 'route-priority-row'; label.textContent = '优先级 ' + (index + 1) + '　' + labels[name];
                    button.type = 'button'; button.textContent = index ? '↑ 上移' : '主线路'; button.disabled = !index;
                    button.onclick = function () {
                        var a = priority.indexOf(name), b = priority.indexOf(active[index - 1]);
                        priority[a] = active[index - 1]; priority[b] = name; refreshFields(); changed();
                    };
                    row.appendChild(label); row.appendChild(button); box.appendChild(row);
                });
            }
            function clientRow(data) {
                var row = document.createElement('div'), mac = document.createElement('input'), select = document.createElement('select'), button = document.createElement('button');
                row.className = 'route-client-row'; mac.type = 'text'; mac.placeholder = '终端 MAC'; mac.value = data.mac || '';
                mac.setAttribute('list', 'c5800-route-hosts'); mac.setAttribute('aria-label', '终端 MAC');
                clientOptions(select, data.uplink); select.setAttribute('aria-label', '终端出口');
                button.type = 'button'; button.textContent = '移除'; button.onclick = function () { row.remove(); rulesDirty = true; changed(); };
                row.appendChild(mac); row.appendChild(select); row.appendChild(button); q('.route-clients').appendChild(row);
            }
            function renderPorts(data) {
                var power = data.power;
                q('[data-power-state]').textContent = power && power.supported ?
                    '当前供电' + (power.enabled ? '已开启。' : '已关闭。') : '';
                all('[data-device]').forEach(function (el) {
                    var port = (data.ports || {})[el.getAttribute('data-device')];
                    el.classList.toggle('link-up', !!(port && port.link));
                    el.querySelector('[data-port-link]').textContent = !port ? '' : port.link ?
                        '已接入' + (port.speed > 0 ? ' · ' + port.speed + ' Mbps' : '') : '未接入';
                });
            }
            function render(data, rules) {
                state = data; mode = data.mode; priority = data.priority.slice();
                renderPorts(data);
                names.forEach(function (name) { if (priority.indexOf(name) < 0) priority.push(name); });
                labels.poe = data.secondary_role === 'broadband' ? 'WAN（4 号口）' : '副 5G';
                names.forEach(function (name) {
                    var line = data.lines[name]; q('[data-line="' + name + '"]').checked = line.enabled;
                    q('[data-weight="' + name + '"]').value = line.weight;
                    q('[data-online="' + name + '"]').textContent = line.enabled ? (line.online ? '在线' : '离线') : '未启用';
                });
                ['proto','ipaddr','netmask','gateway','username','mtu','macaddr'].forEach(function (name) { field(name).value = data.wan[name]; });
                field('password').value = ''; field('peerdns').checked = data.wan.peerdns;
                field('dns').value = Array.from(data.wan.dns || []).join(' '); field('dividing').value = data.dividing;
                refreshFields(); field('default').value = data.default;
                if (rules) {
                    q('.route-clients').textContent = ''; Array.from(rules.clients || []).forEach(clientRow);
                    names.forEach(function (name) { q('[data-address="' + name + '"] textarea').value = Array.from(rules[name] || []).join('\n'); });
                    q('#c5800-route-hosts').textContent = '';
                    Array.from(rules.hosts || []).forEach(function (host) {
                        var item = document.createElement('option'); item.value = host.mac; item.label = (host.name || '') + ' ' + host.ip; q('#c5800-route-hosts').appendChild(item);
                    });
                    rulesDirty = false;
                }
                dirty = false;
                showStatus(data);
            }
            function showStatus(data) {
                status.textContent = titles[data.mode] + ' · ' + names.filter(function (name) { return data.lines[name].enabled; }).map(function (name) {
                    return labels[name] + (data.lines[name].online ? '在线' : '离线');
                }).join(' / ');
            }
            function load() {
                if (pending) return; busy(true); status.textContent = '正在读取线路设置…';
                Promise.all([api('data-route-status'), api('data-dividing')]).then(function (values) {
                    render(values[0], values[1]);
                }).catch(function (e) { status.textContent = e.message; }).then(function () { busy(false); refreshFields(); });
            }
            function changed() { if (!pending) { dirty = true; status.textContent = '设置尚未应用，请点击“保存并应用”'; } }
            all('[data-mode]').forEach(function (b) { b.onclick = function () { mode = b.getAttribute('data-mode'); refreshFields(); changed(); }; });
            all('[data-line], [data-field="proto"], [data-field="dividing"], [data-field="peerdns"]').forEach(function (el) { el.onchange = refreshFields; });
            widget.addEventListener('change', changed);
            widget.addEventListener('input', changed);
            q('.route-details').addEventListener('input', function () { rulesDirty = true; });
            q('.route-details').addEventListener('change', function () { rulesDirty = true; });
            q('[data-add-client]').onclick = function () { clientRow({}); rulesDirty = true; changed(); };
            q('[data-route-refresh]').onclick = load;
            q('[data-route-save]').onclick = function () {
                if (pending || !state) return;
                var value = {mode: names.filter(enabled).length === 1 ? 'Single' : mode, priority: priority.slice(), default: field('default').value, dividing: field('dividing').value, lines: {}, wan: {}};
                names.forEach(function (name) { value.lines[name] = {enabled: enabled(name), weight: Number(q('[data-weight="' + name + '"]').value)}; });
                ['proto','ipaddr','netmask','gateway','username','password','mtu','macaddr'].forEach(function (name) { value.wan[name] = field(name).value.trim(); });
                value.wan.peerdns = field('peerdns').checked; value.wan.dns = field('dns').value.trim().split(/\s+/).filter(Boolean);
                var rules = {clients: all('.route-client-row').map(function (row) { return {mac: row.querySelector('input').value.trim(), uplink: row.querySelector('select').value}; })};
                names.forEach(function (name) { rules[name] = q('[data-address="' + name + '"] textarea').value.split(/\n/); });
                busy(true); status.textContent = '正在保存并应用…';
                (rulesDirty ? api('data-dividing', rules) : Promise.resolve()).then(function () {
                    rulesDirty = false; return api('data-route-set', value);
                }).then(function (data) { render(data); status.textContent = '已应用 · ' + status.textContent; })
                    .catch(function (e) { status.textContent = e.message; }).then(function () { busy(false); refreshFields(); });
            };
            load();
            setInterval(function () {
                if (!state || pending || document.hidden || widget.offsetParent === null) return;
                api('data-route-status').then(function (data) {
                    if (pending) return;
                    renderPorts(data);
                    names.forEach(function (name) {
                        state.lines[name].online = data.lines[name].online;
                        q('[data-online="' + name + '"]').textContent = enabled(name) !== data.lines[name].enabled ?
                            (enabled(name) ? '待启用' : '待停用') : enabled(name) ? (data.lines[name].online ? '在线' : '离线') : '未启用';
                    });
                    if (!dirty) showStatus(data);
                }).catch(function () {});
            }, 5000);
        });
    }
    if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init);
    else init();
}());
