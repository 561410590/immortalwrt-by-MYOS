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
  var border = E('select', { 'class': 'cbi-input-select' }, [
   E('option', { value: 'round' }, '圆角外框'),
   E('option', { value: 'mini' }, '窄边外框'),
   E('option', { value: 'none' }, '无外框')
  ]);
  try { border.value = localStorage.getItem('mwrt-border') || 'round'; } catch (e) {}
  border.addEventListener('change', function() {
   try { localStorage.setItem('mwrt-border', border.value); } catch (e) {}
   if (current !== '/luci-static/argon') return;
   var value = { round: ['5px','14px'], mini: ['3px','7px'], none: ['0px','0px'] }[border.value];
   document.body.style.setProperty('padding', value[0], 'important');
   var main = document.querySelector('.main');
   if (main) main.style.setProperty('border-radius', value[1], 'important');
  });
  return E('div', {}, [
   E('h2', {}, '主题切换'),
   E('div', { 'class': 'cbi-section' }, [
    E('p', {}, 'MYOS 和 MWRT 使用本机对应的首页。应用后页面自动重新载入，可随时切换。'),
    status,
    E('div', { 'class': 'cbi-value' }, [
     E('label', { 'class': 'cbi-value-title', 'for': 'myos-theme-choice' }, '界面主题'),
     E('div', { 'class': 'cbi-value-field' }, [select, ' ', button])
    ])
   ]),
   E('div', { 'class': 'cbi-section' }, [
    E('h3', {}, 'MWRT 外框样式'),
    E('p', {}, '选择后自动记住，使用 MWRT 主题时生效。'),
    border
   ])
  ]);
 },
 handleSave: null,
 handleSaveApply: null,
 handleReset: null
});
