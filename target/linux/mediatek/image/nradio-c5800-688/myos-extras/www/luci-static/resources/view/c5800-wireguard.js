'use strict';
'require view';
'require form';
'require network';
'require protocol.wireguard';
'require uci';
'require rpc';

const handlers = rpc.declare({object:'network', method:'get_proto_handlers'});
const label = 'WireGuard';
function editURL(name) {
    return L.url('admin/network/wireguard') + '?iface=' + encodeURIComponent(name);
}

return view.extend({
    load: function() {
        return Promise.all([network.getNetworks(), handlers(), network.getDevices()]);
    },

    render: function(data) {
        const name = new URLSearchParams(location.search).get('iface');
        const valid = /^[A-Za-z0-9_]{1,15}$/;
        const occupied = id => data[0].some(n => n.getName() === id) || data[2].some(d => d.getName() === id);
        const existing = uci.sections('network', 'interface').filter(s => s.proto === 'wireguard');
        const status = E('a', {class:'btn', href:L.url('admin/status/wireguard')}, '连接状态');
        const top = E('div', {}, [E('h2', {}, label), status]);
        if (!data[1].wireguard)
            top.append(E('p', {class:'alert-message notice'}, '当前网络服务启动于插件安装之前。可以编辑并保存配置，新接口将在网络服务下次启动时生效。'));

        if (!name) {
            for (const iface of existing)
                top.append(E('p', {}, E('a', {href:editURL(iface['.name'])}, iface['.name'])));
            const input = E('input', {type:'text', maxlength:15, placeholder:'wg0', 'aria-label':'新接口名称'});
            const button = E('button', {class:'btn cbi-button', type:'button', click:() => {
                const next = input.value.trim();
                if (!valid.test(next) || uci.get('network', next) || occupied(next)) {
                    input.setCustomValidity('请输入尚未使用的接口名称，可使用 1 至 15 个字母、数字或下划线。');
                    input.reportValidity();
                    return;
                }
                location.href = editURL(next);
            }}, '新建接口');
            input.oninput = () => input.setCustomValidity('');
            top.append(E('p', {}, [input, ' ', button]));
            return top;
        }

        const current = uci.get('network', name);
        if (!valid.test(name) || (current && (current['.type'] !== 'interface' || current.proto !== 'wireguard')) ||
            (!current && occupied(name))) {
            top.append(E('p', {}, '此名称无法用于 WireGuard 接口。'));
            return top;
        }
        if (!current) {
            uci.add('network', 'interface', name);
            uci.set('network', name, 'proto', 'wireguard');
        }
        const map = new form.Map('network');
        const section = map.section(form.NamedSection, name, 'interface', name);
        section.tab('general', '常规设置');
        section.tab('advanced', '高级设置');
        const proto = section.taboption('general', form.ListValue, 'proto', '协议');
        proto.value('wireguard', label);
        proto.readonly = true;
        network.instantiateNetwork(name, 'wireguard').renderFormOptions(section);
        return map.render().then(node => E('div', {}, [top, node]));
    }
});
