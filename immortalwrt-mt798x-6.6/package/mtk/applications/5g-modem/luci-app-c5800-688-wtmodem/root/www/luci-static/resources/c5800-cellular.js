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
            var status = widget.querySelector('.cellular-route-state');
            var buttons = widget.querySelectorAll('[data-route]');
            var pending = false;
            var state = null;
            function controls() {
                buttons.forEach(function (button) {
                    var action = button.getAttribute('data-route');
                    button.disabled = pending || (action !== 'refresh' &&
                        (!state || !state.running || state.mode === action ||
                            (action === 'download' && state.secondary_role !== 'cellular')));
                });
            }
            function request(action) {
                if (pending) return;
                pending = true; controls();
                status.textContent = '正在' + (action === 'refresh' ? '读取' : '应用') + '线路策略…';
                var xhr = new XMLHttpRequest();
                xhr.open(action === 'refresh' ? 'GET' : 'POST', widget.getAttribute(action === 'refresh' ? 'data-route-status' : 'data-route-set'), true);
                xhr.timeout = 10000;
                function failed(message) { pending = false; state = null; status.textContent = message; controls(); }
                xhr.onload = function () {
                    try {
                        var data = JSON.parse(xhr.responseText);
                        if (xhr.status !== 200 || data.error) throw new Error(data.error || '策略应用失败');
                        state = data;
                        var secondary = data.secondary_role === 'broadband' ? '有线 WAN' : '副 5G';
                        widget.querySelector('[data-route="daily"]').textContent = '日常 · ' + secondary + ' 优先';
                        status.textContent = !data.running ? '线路服务未启动' :
                            (data.mode === 'download' ? '多连接下载分流' : '日常 · ' + secondary + ' 优先') +
                            ' · 内置 ' + (data.main_online ? '在线' : '离线') + ' / ' + secondary + ' ' + (data.poe_online ? '在线' : '离线');
                        pending = false; controls();
                    } catch (e) { failed(e.message); }
                };
                xhr.onerror = xhr.ontimeout = function () { failed('请求超时或连接失败，请刷新状态'); };
                if (action === 'refresh') xhr.send();
                else {
                    xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
                    xhr.send('mode=' + action + '&token=' + encodeURIComponent(widget.getAttribute('data-token')));
                }
            }
            buttons.forEach(function (button) { button.addEventListener('click', function () { request(button.getAttribute('data-route')); }); });
            request('refresh');
        });
    }
    if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init);
    else init();
}());
