'use strict';
'require baseclass';
'require dom';
'require firewall';
'require ui';

const ssidOf = net => net.getSSID() || net.getShortName();
const field = (name, value) => E('div', [ E('dt', name), E('dd', [value ?? '—']) ]);

class WirelessStations {
	constructor(formatRate) {
		this.formatRate = formatRate;
		this.rows = new Map();
		this.ssids = '';
		this.filter = E('select', { 'aria-label': '按 SSID 筛选' }, [
			E('option', { value: '' }, '全部 SSID')
		]);
		this.filter.hidden = true;
		this.filter.addEventListener('change', () => this.applyFilter());
		this.list = E('div', { class: 'm-wireless-list' });
		this.empty = E('p', { class: 'm-wireless-empty', role: 'status' }, _('Collecting data...'));
		this.element = E('section', { id: 'wifi_assoclist_table', class: 'm-wireless-stations' }, [
			E('div', { class: 'm-wireless-heading' }, [ E('h3', _('Associated Stations')), this.filter ]),
			this.list, this.empty
		]);
	}

	applyFilter() {
		const ssid = this.filter.value;
		let visible = 0;
		for (const row of this.rows.values()) {
			row.element.hidden = ssid !== '' && row.ssid !== ssid;
			if (!row.element.hidden)
				visible++;
		}
		this.empty.hidden = visible > 0;
		if (!visible)
			this.empty.textContent = ssid ? '此 SSID 暂无关联终端' : '暂无关联终端';
	}

	createRow() {
		const name = E('strong');
		const ssid = E('span', { class: 'm-wireless-ssid' });
		const signal = E('span', { class: 'm-wireless-signal' });
		const info = E('dl');
		const button = E('button', { type: 'button', class: 'cbi-button cbi-button-remove' }, _('Disconnect'));
		const summary = E('summary', [
			E('span', { class: 'm-wireless-identity' }, [ name, ssid ]),
			signal
		]);
		const element = E('details', { class: 'm-wireless-station' }, [
			summary, E('div', { class: 'm-wireless-details' }, [ info, button ])
		]);
		const row = { name, ssidLabel: ssid, signal, info, button, element };
		button.addEventListener('click', async () => {
			const bss = row.station;
			summary.focus();
			button.disabled = true;
			button.classList.add('spinning');
			try {
				await bss.network.disconnectClient(bss.mac, true, 5, 60000);
			}
			catch (error) {
				ui.addNotification(null, E('p', error.message));
			}
			finally {
				button.classList.remove('spinning');
				button.disabled = row.readonly;
			}
		});
		return row;
	}

	update(hosts, wifiNetworks, stations, zones, readonly) {
		const ssids = [...new Set(wifiNetworks.map(ssidOf))].sort();
		const ssidList = JSON.stringify(ssids);
		if (ssidList !== this.ssids) {
			const selected = this.filter.value;
			dom.content(this.filter, [
				E('option', { value: '' }, '全部 SSID'),
				...ssids.map(ssid => E('option', { value: ssid }, ssid))
			]);
			this.filter.value = ssids.length > 1 && ssids.includes(selected) ? selected : '';
			this.filter.hidden = ssids.length < 2;
			this.ssids = ssidList;
		}

		const active = new Set();
		for (const bss of stations) {
			const key = bss.network.getName() + '/' + bss.mac;
			active.add(key);
			let row = this.rows.get(key);
			if (!row) {
				row = this.createRow();
				this.rows.set(key, row);
				this.list.append(row.element);
			}

			const ipv4 = hosts.getIPAddrByMACAddr(bss.mac);
			const ipv6 = hosts.getIP6AddrByMACAddr(bss.mac);
			const address = [ipv4, ipv6].filter(Boolean).join(' · ');
			row.station = bss;
			row.ssid = ssidOf(bss.network);
			row.readonly = readonly;
			row.name.textContent = hosts.getHostnameByMACAddr(bss.mac) || ipv4 || ipv6 || bss.mac;
			row.ssidLabel.textContent = row.ssid;
			row.signal.textContent = Number.isFinite(bss.signal) && bss.signal < 0 ? bss.signal + ' dBm' : '—';
			row.button.hidden = !bss.network.isClientDisconnectSupported();
			row.button.disabled = readonly || row.button.classList.contains('spinning');

			const info = [
				field(_('MAC address'), bss.mac),
				field(_('IP address'), address || null),
				field(_('Network'), bss.network.getIfname()),
				field('SNR', Number.isFinite(bss.snr) ? bss.snr + ' dB' : null),
				field(_('RX Rate'), bss.rx?.rate != null ? this.formatRate(bss.rx) : null),
				field(_('TX Rate'), bss.tx?.rate != null ? this.formatRate(bss.tx) : null),
				field('收包', bss.rx?.packets),
				field('发包', bss.tx?.packets)
			];
			if (bss.connected_time > 0)
				info.push(field(_('Uptime'), '%t'.format(bss.connected_time)));
			if (Number.isFinite(bss.noise) && bss.noise < 0)
				info.push(field(_('Noise'), bss.noise + ' dBm'));
			if (bss.vlan) {
				const zone = zones?.find(item => item.getNetworks().includes(bss.vlan.getNetwork()));
				info.push(field('VLAN', E('span', {
					class: 'zonebadge', style: firewall.getZoneColorStyle(zone)
				}, bss.vlan.getI18n())));
			}

			dom.content(row.info, info);
		}

		for (const [key, row] of this.rows) {
			if (!active.has(key)) {
				row.element.remove();
				this.rows.delete(key);
			}
		}

		this.applyFilter();
		if (!this.openedFromHome && this.element.isConnected && location.hash === '#wifi_assoclist_table') {
			this.openedFromHome = true;
			this.element.scrollIntoView({ block: 'start' });
		}
	}
}

return baseclass.extend({
	create(formatRate) {
		return new WirelessStations(formatRate);
	}
});
