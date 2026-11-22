'use strict';
'require view';

return view.extend({
    render: function() {
        const select = E('select', { id: 'interface-theme', class: 'cbi-input-select' }, [
            E('option', { value: 'myos' }, 'MYOS'),
            E('option', { value: 'nros' }, 'NROS 原厂')
        ]);
        const button = E('button', {
            type: 'button',
            class: 'cbi-button cbi-button-action',
            click: function() {
                if (select.value === 'myos') {
                    location.assign(L.url('admin', 'myos'));
                    return;
                }
                const url = new URL('/cgi-bin/luci/', location.href);
                url.protocol = 'http:';
                url.port = '18083';
                location.assign(url.href);
            }
        }, '切换');
        return E('div', {}, [
            E('h2', {}, '主题切换'),
            E('div', { class: 'cbi-section' }, [
                E('label', { for: 'interface-theme' }, '界面主题'),
                select, ' ', button
            ])
        ]);
    },
    handleSave: null,
    handleSaveApply: null,
    handleReset: null
});
