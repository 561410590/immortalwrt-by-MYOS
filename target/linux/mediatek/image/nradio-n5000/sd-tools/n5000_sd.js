'use strict';
'require view';
'require rpc';
'require ui';
var getStatus=rpc.declare({object:'myos.ultra.sd',method:'status'});
var returnStock=rpc.declare({object:'myos.ultra.sd',method:'stock',params:['confirmed']});
function capacity(n) {
 return typeof n==='number' && n>0 ? (n/1073741824).toFixed(2)+' GiB' : '—';
}
var errors={
 not_n5000_sd:'当前设备不是受支持的 N5000 SD 系统。',
 cannot_open_sd:'SD 卡不可访问。',
 unexpected_ultra_gpt:'SD 分区布局与 N5000 镜像不一致。',
 backup_gpt_invalid:'GPT 备份未就绪，请先确认自动扩容已完成。',
 overlay_not_ready:'SD overlay 尚未就绪。',
 backup_failed:'分区表备份失败。',
 sd_busy:'SD 分区表正在使用，请稍后重试。',
 gpt_write_failed:'分区名切换失败，请保留 SD 卡并使用恢复工具检查。',
 helper_failed:'功能服务未返回有效状态。',
 helper_unavailable:'功能服务不可用。'
};
return view.extend({
 load:function() { return getStatus(); },
 render:function(data) {
  data=data||{};
  var nativeLayout=data.layout==='official';
  var button=E('button',{
   'class':'cbi-button cbi-button-action important',
   'disabled':!data.ready || data.mode!=='myos' || this.readonly,
   'click':function() {
    ui.showModal('回官方系统',[
     E('p',{},'切换 SD 启动状态并重启，官方系统配置保留。'),
     E('p',{},nativeLayout?'重返 MYOS 可在官方存储管理中选择第二系统优先。':'切换后保持官方启动。重返 MYOS 需使用下方恢复工具还原 SD 启动状态。'),
     E('p',{},'切换后设备将重启，请先下载恢复工具。'),
     E('div',{'class':'right'},[
      E('button',{'class':'cbi-button','click':ui.hideModal},'取消'),
      ' ',
      E('button',{'class':'cbi-button cbi-button-action important','click':function(ev) {
       ev.currentTarget.disabled=true;
       return returnStock(true).then(function(result) {
        if(!result || !result.ok) throw new Error(errors[result&&result.error] || '切换失败');
        ui.showModal('正在重启',[
         E('p',{},'SD 启动状态已切换。约 5 秒后重启，请使用官方系统地址访问。')
        ]);
       }).catch(function(err) {
        ui.hideModal(); ui.addNotification(null,E('p',{},err.message),'error');
       });
      }},'切换并重启')
     ])
    ]);
   }
  },'一键回官方系统');
  return E('div',{'class':'cbi-map'},[
   E('h2',{},'N5000 · SD 扩容与回官方'),
   E('p',{},nativeLayout?'系统存储按官方分区识别；整卡迁移到更大 SD 后，启动时扩展第二系统尾部空间。':'SD 剩余空间自动接入系统存储，已有 MYOS 数据分区保留并扩展容量。'),
   E('table',{'class':'table'},[
    E('tr',{'class':'tr'},[E('td',{'class':'td'},'SD 卡容量'),E('td',{'class':'td'},capacity(data.sd_bytes))]),
    E('tr',{'class':'tr'},[E('td',{'class':'td'},'系统分区容量'),E('td',{'class':'td'},capacity(data.data_bytes || data.p7_bytes))]),
    E('tr',{'class':'tr'},[E('td',{'class':'td'},'Overlay 容量 / 可用'),E('td',{'class':'td'},capacity(data.overlay_bytes)+' / '+capacity(data.available_bytes))]),
    E('tr',{'class':'tr'},[E('td',{'class':'td'},'启动状态'),E('td',{'class':'td'},data.ok?(data.mode==='stock'?'官方':data.mode==='myos'?'MYOS':'未知'):(errors[data.error] || '状态不可用'))])
   ]),
   E('p',{},E('a',{'href':'/myos-n5000-recovery.tar','download':'myos-n5000-recovery.tar'},'下载回 MYOS 恢复工具与说明')),
   E('p',{},nativeLayout?'官方存储管理支持直接选择第二系统优先。':'恢复工具同时保存在 SD 数据分区的 myos-recovery 目录。'),
   button
  ]);
 },
 handleSave:null,handleSaveApply:null,handleReset:null
});
