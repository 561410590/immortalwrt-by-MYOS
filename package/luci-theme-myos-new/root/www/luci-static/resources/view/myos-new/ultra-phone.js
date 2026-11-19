'use strict';
'require view';
'require view.myos-new.ultra-tools as ultraTools';

return view.extend({
	render() {
		const phone = ultraTools.createPhone();
		setTimeout(phone.start, 0);
		return E('div', {class:'cbi-map m-ultra-phone-page'}, [
			E('link', {rel:'stylesheet', href:L.resource('view','myos-new','ultra-phone.css') + '?v=2'}),
			E('h2', {}, ['电话 ', E('span', {class:'m-badge'}, '（实验性）')]),
			phone.form
		]);
	},
	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
