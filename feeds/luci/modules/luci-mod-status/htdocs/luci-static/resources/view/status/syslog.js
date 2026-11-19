'use strict';
'require view';
'require fs';
'require poll';
'require ui';

return view.extend({
	logFacilityFilter: 'any',
	invertLogFacilitySearch: false,
	logSeverityFilter: 'any',
	invertLogseveritySearch: false,
	logTextFilter: '',
	invertLogTextSearch: false,

	facilities: [
		['any', '', _('Any')],
		['0',  'kern',   _('Kernel')],
		['1',  'user',   _('User')],
		['2',  'mail',   _('Mail')],
		['3',  'daemon', _('Daemon')],
		['4',  'auth',   _('Auth')],
		['5',  'syslog', _('Syslog')],
		['6',  'lpr',    _('LPR')],
		['7',  'news',   _('News')],
		['8',  'uucp',   _('UUCP')],
		['9',  'cron',   _('Cron')],
		['10', 'authpriv', _('Auth Priv')],
		['11', 'ftp', _('FTP')],
		['12', 'ntp', _('NTP')],
		['13', 'security', _('Log audit')],
		['14', 'console', _('Log alert')],
		['15', 'cron', _('Scheduling daemon')],
		['16', 'local0', _('Local 0')],
		['17', 'local1', _('Local 1')],
		['18', 'local2', _('Local 2')],
		['19', 'local3', _('Local 3')],
		['20', 'local4', _('Local 4')],
		['21', 'local5', _('Local 5')],
		['22', 'local6', _('Local 6')],
		['23', 'local7', _('Local 7')]
	],

	severity: [
		['any', '', _('Any')],
		['0',  'emerg',   _('Emergency')],
		['1',  'alert',   _('Alert')],
		['2',  'crit',   _('Critical')],
		['3',  'err', _('Error')],
		['4',  'warn',   _('Warning')],
		['5',  'notice', _('Notice')],
		['6',  'info',    _('Info')],
		['7',  'debug',   _('Debug')]
	],


	retrieveLog: async function() {
		const facility = this.logFacilityFilter;

		return Promise.all([
			L.resolveDefault(fs.stat('/usr/libexec/syslog-wrapper'), null),
		]).then((stat) => {
			const logger = stat[0]?.path;

			return fs.exec_direct(logger).then(logdata => {
				let loglines = logdata.trim().split(/\n/);

				// Filter by facility, and additionally severity string if selected
				if (this.logSeverityFilter !== 'any') {
					const sev = this.logSeverityFilter?.toLowerCase?.();
					const fac = this.logFacilityFilter === 'any'
						? this.facilities.map(f => f[1]) // all facility short names
						: [ this.facilities.find(f => f[0] === this.logFacilityFilter)?.[1] ];

					loglines = loglines.filter(line => {
						const sevMatch = this.logSeverityFilter === 'any' || fac.some(facility => line.includes(`.${sev}`));
						const facMatch = this.logFacilityFilter === 'any' || fac.some(facility => line.includes(`${facility}.`));

						const finalMatch = (this.invertLogseveritySearch ? !sevMatch : sevMatch)
						               && (this.invertLogFacilitySearch ? !facMatch : facMatch);

						return finalMatch;
					});

				}

				loglines = loglines.filter(line => {
					const match = line.includes(this.logTextFilter);
					return this.invertLogTextSearch ? !match : match;
				});

				return { 
					value: loglines.join('\n'),
					rows: loglines.length + 1
				};
			}).catch(function(err) {
				ui.addNotification(null, E('p', {}, _('Unable to load log data: ' + err.message)));
				return { 
					value: '',
					rows: 0
				};
			});
		});
	},

	pollLog: async function() {
		const element = document.getElementById('syslog');
		if (element) {
			const log = await this.retrieveLog();
			if (!document.body.classList.contains('m-submenu-compact')) {
				element.value = log.value;
				element.rows = log.rows;
			}
			else if (element.value !== log.value) {
				const top = element.scrollTop;
				const atTail = top > 0 && element.scrollHeight - top - element.clientHeight <= 2;
				const start = element.selectionStart, end = element.selectionEnd;
				element.value = log.value;
				if (document.activeElement === element)
					element.setSelectionRange(start, end);
				element.scrollTop = atTail ? element.scrollHeight : top;
			}
		}
	},

	load: async function() {
		poll.add(this.pollLog.bind(this));
		return await this.retrieveLog();
	},

	render: function(loglines) {
		const compact = document.body.classList.contains('m-submenu-compact');
		const scrollDownButton = E('button', {
				'id': 'scrollDownButton',
				'class': 'cbi-button cbi-button-neutral'
			}, _('Scroll to tail', 'scroll to bottom (the tail) of the log file')
		);
		scrollDownButton.addEventListener('click', () => {
			if (!compact)
				return scrollUpButton.scrollIntoView();
			const log = document.getElementById('syslog');
			log.scrollTop = log.scrollHeight;
		});

		const scrollUpButton = E('button', {
				'id' : 'scrollUpButton',
				'class': 'cbi-button cbi-button-neutral'
			}, _('Scroll to head', 'scroll to top (the head) of the log file')
		);
		scrollUpButton.addEventListener('click', () => {
			if (!compact)
				return scrollDownButton.scrollIntoView();
			document.getElementById('syslog').scrollTop = 0;
		});

		const self = this;

		// Create facility invert checkbox
		const facilityInvert = E('input', {
			'id': 'invertLogFacilitySearch',
			'type': 'checkbox',
			'class': 'cbi-input-checkbox',
		});

		// Create facility select-dropdown from facilities map
		const facilitySelect = E('select', {
			'id': 'logFacilitySelect',
			'class': 'cbi-input-select',
			'style': 'margin-bottom:10px',
		},
		this.facilities.map(([val, _, label]) =>
			E('option', { value: val }, label)
		));

		// Create severity invert checkbox
		const severityInvert = E('input', {
			'id': 'invertLogseveritySearch',
			'type': 'checkbox',
			'class': 'cbi-input-checkbox',
		});

		// Create severity select-dropdown from facilities map
		const severitySelect = E('select', {
			'id': 'logSeveritySelect',
			'class': 'cbi-input-select',
		},
		this.severity.map(([_, val, label]) =>
			E('option', { value: val }, label)
		));

		// Create raw text search invert checkbox
		const filterTextInvert = E('input', {
			'id': 'invertLogTextSearch',
			'type': 'checkbox',
			'class': 'cbi-input-checkbox',
		});

		// Create raw text search text input
		const filterTextInput = E('input', {
			'id': 'logTextFilter',
			'class': 'cbi-input-text',
		});

		function handleLogFilterChange() {
			self.logFacilityFilter = facilitySelect.value;
			self.invertLogFacilitySearch = facilityInvert.checked;
			self.logSeverityFilter = severitySelect.value;
			self.invertLogseveritySearch = severityInvert.checked;
			self.logTextFilter = filterTextInput.value;
			self.invertLogTextSearch = filterTextInvert.checked;
			self.pollLog();
		}

		facilitySelect.addEventListener('change', handleLogFilterChange);
		facilityInvert.addEventListener('change', handleLogFilterChange);
		severitySelect.addEventListener('change', handleLogFilterChange);
		severityInvert.addEventListener('change', handleLogFilterChange);
		filterTextInput.addEventListener('input', handleLogFilterChange);
		filterTextInvert.addEventListener('change', handleLogFilterChange);

		if (compact) {
			const label = (name, input, cls) => E('label', { for: input.id, class: cls || '' }, [name, input]);
			const invert = (name, input) => E('label', { class: 'm-log-invert' }, [input, name]);
			return E([], [
				E('h2', _('System Log')),
				E('div', { id: 'content_syslog' }, [
					E('div', { class: 'm-log-toolbar' }, [
						label('级别', severitySelect),
						label('关键词', filterTextInput, 'm-log-keyword'),
						E('div', { class: 'm-log-jump' }, [scrollUpButton, scrollDownButton])
					]),
					E('details', { class: 'm-log-options' }, [
						E('summary', '更多筛选'),
						E('div', { class: 'm-log-toolbar' }, [
							label('来源', facilitySelect),
							invert('排除所选来源', facilityInvert),
							invert('排除所选级别', severityInvert),
							invert('排除关键词', filterTextInvert)
						])
					]),
					E('textarea', { id: 'syslog', readonly: '', wrap: 'soft', rows: 20, 'aria-label': _('System Log') }, loglines.value)
				])
			]);
		}

		return E([], [
			E('h2', {}, [ _('System Log') ]),
			E('div', { 'id': 'content_syslog' }, [
				E('div', { 'style': 'margin-bottom:10px' }, [
					E('label', { 'for': 'invertLogFacilitySearch', 'style': 'margin-right:5px' }, _('Not')),
					facilityInvert,
					E('label', { 'for': 'logFacilitySelect', 'style': 'margin: 0 5px' }, _('facility:')),
					facilitySelect,
					E('label', { 'for': 'invertLogseveritySearch', 'style': 'margin: 0 5px' }, _('Not')),
					severityInvert,
					E('label', { 'for': 'logSeveritySelect', 'style': 'margin: 0 5px' }, _('severity:')),
					severitySelect,
				]),
				E('div', { 'style': 'margin-bottom:10px' }, [
					E('label', { 'for': 'invertLogTextSearch', 'style': 'margin-right:5px' }, _('Not')),
					filterTextInvert,
					E('label', { 'for': 'logTextFilter', 'style': 'margin: 0 5px' }, _('including:')),
					filterTextInput,
				]),
				E('div', {'style': 'padding-bottom: 20px'}, [scrollDownButton]),
				E('textarea', {
					'id': 'syslog',
					'style': 'font-size:12px',
					'readonly': 'readonly',
					'wrap': 'off',
					'rows': loglines.rows,
				}, [ loglines.value ]),
				E('div', {'style': 'padding-bottom: 20px'}, [scrollUpButton])
			])
		]);
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
