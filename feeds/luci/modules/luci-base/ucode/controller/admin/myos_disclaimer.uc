import { access, readfile, writefile } from 'fs';

const accepted_path = '/etc/myos-disclaimer.accepted';

function reply(status, message, accepted) {
	http.status(status, message);
	http.prepare_content('application/json');
	http.write_json({ accepted });
}

return {
	handle: function() {
		if (access(accepted_path))
			return reply(200, 'OK', true);

		let action = http.formvalue('action');
		if (action != 'start' && (action != 'accept' || http.formvalue('agree') != '1'))
			return reply(400, 'Invalid consent', false);

		let uptime = readfile('/proc/uptime');
		if (uptime == null)
			return reply(503, 'Clock unavailable', false);

		let now = int(+split(uptime, ' ')[0] * 100);
		let session = ubus.call('session', 'get', {
			ubus_rpc_session: ctx.authsession,
			keys: [ 'myos_disclaimer_start' ]
		});
		let started = session?.values?.myos_disclaimer_start;

		if (action == 'start') {
			if (started == null) {
				ubus.call('session', 'set', {
					ubus_rpc_session: ctx.authsession,
					values: { myos_disclaimer_start: now }
				});
				if (ubus.error(true))
					return reply(503, 'Session unavailable', false);
			}
			return reply(200, 'OK', false);
		}

		if (started == null || now - started < 1000)
			return reply(409, 'Waiting period incomplete', false);

		if (!writefile(accepted_path, 'accepted\n'))
			return reply(500, 'Unable to save consent', false);

		return reply(200, 'OK', true);
	}
};
