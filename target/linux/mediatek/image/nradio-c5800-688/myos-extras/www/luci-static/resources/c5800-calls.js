(() => {
    'use strict';
    const tag = document.currentScript;
    const status = document.getElementById('c5800-call-status');
    let busy = false, lastHistory, lastMissed, notice;
    function banner(title, number) {
        if (!notice) {
            notice = document.createElement('a');
            notice.className = 'alert-message notice';
            notice.style.cssText = 'position:fixed;right:1rem;bottom:1rem;z-index:12000;max-width:80vw';
            notice.setAttribute('role', 'alert');
            notice.href = tag.dataset.page;
            document.body.append(notice);
        }
        notice.textContent = title + ' · ' + (number || '未知号码');
        notice.hidden = false;
    }
    function history(rows) {
        const body = document.getElementById('c5800-missed');
        body.replaceChildren();
        for (const item of rows) {
            const row = body.insertRow();
            row.className = 'tr';
            for (const text of [new Date(item.time * 1000).toLocaleString('zh-CN', {hour12:false}), item.number || '未知号码']) {
                const cell = row.insertCell();
                cell.className = 'td';
                cell.textContent = text;
            }
        }
        document.getElementById('c5800-no-missed').hidden = rows.length > 0;
    }
    let missedUntil = 0;
    async function refresh() {
        if (busy || document.hidden) return;
        busy = true;
        try {
            const response = await fetch(tag.dataset.state + (status ? '?history=1' : ''), {cache:'no-store'});
            if (!response.ok) return;
            const state = await response.json();
            const ring = state.available && state.ringing && state.ringing[0];
            const latest = state.latest?.id || '';
            if (ring) {
                banner('MT5700 来电', ring.number);
            } else if (lastMissed !== undefined && latest && latest !== lastMissed) {
                banner('未接来电', state.latest.number);
                missedUntil = Date.now() + 15000;
            } else if (notice && Date.now() >= missedUntil) {
                notice.hidden = true;
            }
            lastMissed = latest;
            if (status) {
                status.textContent = !state.available ? 'MT5700 来电状态暂不可读' :
                    ring ? '正在振铃 · ' + (ring.number || '未知号码') : 'MT5700 正在监听来电';
                document.getElementById('c5800-call-ims').textContent = state.ims === 0 ? 'IMS 开关已关闭' :
                    state.registered === 0 ? 'IMS 尚未注册' : state.registered === 1 ? 'IMS 已注册' : '';
                if (lastHistory !== latest) history(Array.isArray(state.history) ? state.history : []);
                lastHistory = latest;
            }
        } catch (_) {
            if (status) status.textContent = '暂时无法读取来电状态';
        } finally { busy = false; }
    }
    refresh();
    setInterval(refresh, 3000);
    document.addEventListener('visibilitychange', refresh);
})();
