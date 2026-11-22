'use strict';
'require view';

return view.extend({
    render: function() {
        const theme = E('select', { id: 'c5800-theme', class: 'cbi-input-select' }, [
            E('option', { value: 'myos' }, 'MYOS'),
            E('option', { value: 'nros' }, 'NROS 原厂')
        ]);
        const apply = E('button', {
            type: 'button',
            class: 'cbi-button cbi-button-action',
            click: function() {
                if (theme.value === 'nros') {
                    const url = new URL(location.href);
                    url.protocol = 'http:';
                    url.port = '18083';
                    url.pathname = '/cgi-bin/luci/';
                    url.search = '';
                    url.hash = '';
                    location.assign(url.href);
                    return;
                }
                location.assign(L.url('admin', 'myos'));
            }
        }, '切换');
        return E('div', {}, [
            E('h2', {}, '主题切换'),
            E('div', { class: 'cbi-section' }, [
                E('div', { class: 'cbi-value' }, [
                    E('label', { for: 'c5800-theme', class: 'cbi-value-title' }, '界面主题'),
                    E('div', { class: 'cbi-value-field' }, [theme, ' ', apply])
                ])
            ])
        ]);
    },
    handleSave: null,
    handleSaveApply: null,
    handleReset: null
});
