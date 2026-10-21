'use strict';
'require view';
'require uci';
'require rpc';
'require ui';

var commitTheme = rpc.declare({
 object: 'uci',
 method: 'commit',
 params: ['config'],
 reject: true
});

return view.extend({
 load: function() {
  return uci.load('luci');
 },
 render: function() {
  var readonly = !L.hasViewPermission();
  var applying = false;
  var current = uci.get('luci', 'main', 'mediaurlbase');
  var status = E('p', { 'class': 'alert-message error', 'role': 'alert', 'style': 'display:none' });
  var select = E('select', { 'class': 'cbi-input-select', 'id': 'myos-theme-choice', 'disabled': readonly ? '' : null }, [
   E('option', { value: '/luci-static/c2000max-ui' }, 'MYOS'),
   E('option', { value: '/luci-static/argon' }, 'MWRT')
  ]);
  select.value = current === '/luci-static/argon' ? current : '/luci-static/c2000max-ui';
  var button = E('button', {
   'type': 'button',
   'class': 'cbi-button cbi-button-apply',
   'disabled': readonly ? '' : null,
   'click': ui.createHandlerFn(this, function() {
    if (readonly || applying) return;
    applying = true;
    status.style.display = 'none';
    status.textContent = '';
    button.disabled = true;
    select.disabled = true;
    uci.set('luci', 'main', 'mediaurlbase', select.value);
    return uci.save().then(function() {
     return commitTheme('luci');
    }).then(function() {
     ui.menu.flushCache();
     window.location.replace(L.url('admin') + '?theme=' + Date.now());
    }).catch(function(err) {
     applying = false;
     button.disabled = readonly;
     select.disabled = readonly;
     status.textContent = '主题保存失败：' + (err.message || String(err));
     status.style.display = '';
    });
   })
  }, '应用主题');
  return E('div', {}, [
   E('h2', {}, '主题切换'),
   E('div', { 'class': 'cbi-section' }, [
    E('p', {}, 'MYOS 和 MWRT 使用本机对应的首页。应用后页面自动重新载入，可随时切换。'),
    status,
    E('div', { 'class': 'cbi-value' }, [
     E('label', { 'class': 'cbi-value-title', 'for': 'myos-theme-choice' }, '界面主题'),
     E('div', { 'class': 'cbi-value-field' }, [select, ' ', button])
    ])
   ])
  ]);
 },
 handleSave: null,
 handleSaveApply: null,
 handleReset: null
});
