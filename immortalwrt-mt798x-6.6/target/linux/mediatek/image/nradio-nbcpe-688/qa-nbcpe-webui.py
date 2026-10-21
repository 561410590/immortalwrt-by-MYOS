#!/usr/bin/env python3
"""Check the installed Web UI protocol without modem or network access."""
import argparse
import ast
import asyncio
import json
import runpy
import sys
import types
from datetime import datetime
from pathlib import Path
from unittest.mock import patch


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', required=True, type=Path)
    parser.add_argument('--www', type=Path)
    args = parser.parse_args()
    root = args.root
    backend = root / 'usr/libexec/myos-nbcpe-webui'
    ast.parse(backend.read_text(encoding='utf-8'))
    view = (root / 'usr/lib/lua/luci/view/zmode-AK68/webui-AK68.htm').read_text(encoding='utf-8')
    assert 'src="/5700/?embed=1"' in view and 'get_ip' not in view and '192.168.66.1' not in view
    assert 'event.source !== frame.contentWindow' in view and 'event.origin !== location.origin' in view
    assert 'cellular-menu' in view and 'max-width:100%' in view
    static = (args.www or root / 'www') / '5700'
    html_files = list(static.rglob('*.html'))
    assert html_files and (static / 'index.html').is_file()
    for path in html_files:
        text = path.read_text(encoding='utf-8')
        assert 'location.hostname' in text and '/scripts/loading.js"' not in text.replace('/5700/scripts/loading.js"', '')
        assert "localStorage.setItem('atPort', '8766')" in text
        assert "if (!localStorage.getItem('atPort'))" not in text
        assert '/5700/nbcpe-protocol.js' in text
        assert 'src="/5700/umi.ec9b4b52.js"' in text
        assert 'href="/5700/umi.31851818.css"' in text
        assert 'src="/5700/preload_helper.66ca4a74.js"' in text
        assert 'myos-nbcpe-webui-height' in text and 'ResizeObserver' in text
        assert '.ant-drawer-open .ant-drawer-mask' in text and 'if (mask) mask.click()' in text
    for path in static.glob('*.js'):
        assert '"192.168.66.1"' not in path.read_text(encoding='utf-8')
    assert json.loads((static / 'config.json').read_text(encoding='utf-8')) == {'at': {'host': '', 'port': 8766}, 'status': 'false'}
    provenance = json.loads((static / 'mt5700-webui-upstream.json').read_text(encoding='utf-8'))
    assert provenance['version'] == '2.2.2' and provenance['commit'] == '7abccd44c81442b13dbf974ad9a0aa67cf527e90'
    bundle = (static / 'umi.ec9b4b52.js').read_text(encoding='utf-8')
    assert sorted(path.name for path in static.glob('umi.*.js')) == ['umi.ec9b4b52.js']
    assert sorted(path.name for path in static.glob('umi.*.css')) == ['umi.31851818.css']
    assert sorted(path.name for path in static.glob('preload_helper.*.js')) == ['preload_helper.66ca4a74.js']
    assert 'I()(this,"commandTimeout",15e3)' in bundle and 'window.NBCPE.acceptEnvelope(this,S)' in bundle
    assert 'this.commandQueue=this.commandQueue.then' in bundle and 'this.host=location.hostname,this.port=8766' in bundle
    assert 'at:{host:location.hostname,port:8766},status:"true"' in bundle
    assert 'L.connected=false,L.ws&&L.ws.close()' in bundle
    assert 'L.smsCollecting=!1,L.smsResolve=null,L.smsBuffer=[],L.connected=false,L.ws&&L.ws.close()' in bundle
    assert 'clearTimeout(this.smsTimer);this.smsTimer=null;this.smsCollecting=false;this.smsBuffer=[]' in bundle
    network = (static / 'p__CPE__Network__Info__index.30901ff8.async.js').read_text(encoding='utf-8')
    assert 'window.NBCPE.connection(E)' in network and 'AT^DSAMBR=8' not in network
    assert 'AT^DSFLOWQRY="+(window.NBCPE.activeCid||8)' in network
    assert 'AT+CGEQOSRDP=1' not in network
    assert 'w=g,y=parseInt(w[2])' in network and 'I=t,w=parseInt(I[2])' in network
    settings = (static / 'p__CPE__Network__Settings__index.893ddec9.async.js').read_text(encoding='utf-8')
    assert 'window.NBCPE.resolveScs(D,r,le(r.band))' in settings and 'window.NBCPE.verifyCellLock(reply,r)' in settings
    for path in static.glob('p__CPE__*.js'):
        assert '.disconnect()}' not in path.read_text(encoding='utf-8'), path.name

    stock = types.SimpleNamespace(
        handle_cmgl_response=lambda text: 'decoded:' + text,
        SMSDeliver=types.SimpleNamespace(decode=lambda text: {
            'sender': {'number': '12345'}, 'user_data': {'data': '短信'},
            'scts': datetime(2026, 10, 9, 12, 34, 56)}))
    spec = types.SimpleNamespace(loader=types.SimpleNamespace(exec_module=lambda module: None))
    websocket_module = types.ModuleType('websockets')
    class ConnectionClosed(Exception):
        pass
    websocket_module.exceptions = types.SimpleNamespace(ConnectionClosed=ConnectionClosed)
    with patch.dict(sys.modules, {'websockets': websocket_module}), \
            patch('importlib.util.spec_from_file_location', return_value=spec), \
            patch('importlib.util.module_from_spec', return_value=stock):
        module = runpy.run_path(str(backend))
    argv = []

    class Process:
        returncode = 0

        def __init__(self, stdout=b'AT+CFUN?\r\n+CFUN: 1\r\nOK\r\n', stderr=b'', status=0):
            self.stdout, self.stderr, self.returncode = stdout, stderr, status

        async def communicate(self):
            return self.stdout, self.stderr

    async def create_process(*arguments, **options):
        assert 'shell' not in options
        argv.append(arguments)
        return Process()

    async def fixtures():
        with patch('asyncio.create_subprocess_exec', side_effect=create_process):
            assert await module['execute']('AT+CFUN?\r') == '+CFUN: 1'
            assert argv[-1] == ('/usr/sbin/atsd_cli', '-i', 'cpe', '-c', 'AT+CFUN?')
            pdu = 'AT+CMGW=12\r0011223344\x1a'
            await module['execute'](pdu)
            assert argv[-1][-1] == pdu
        assert module['filtered_response']('AT+CMGL=4', 'PDU\r\nOK\r\n') == 'decoded:PDU\r\nOK\r\n'
        assert module['filtered_response']('AT+CFUN=1', 'AT+CFUN=1\r\nOK\r\n') == 'OK'
        decoded = module['filtered_response']('AT+CMGR=7', '+CMGR: 0,,23\r\n0011\r\nOK\r\n')
        assert decoded == '+CMGR: "0","+12345","","26/10/09 12:34:56"\n短信'
        for process, expected in [(Process(b'', b'not ready', 1), 'not ready'),
                                  (Process(b'', b'', 0), 'empty response')]:
            async def failed_process(*arguments, **options):
                return process
            with patch('asyncio.create_subprocess_exec', side_effect=failed_process):
                try:
                    await module['execute']('ATI')
                    raise AssertionError('failed AT command reported success')
                except RuntimeError as error:
                    assert expected in str(error)

        class WebSocket:
            def __init__(self):
                self.input = iter(['ping', 'AT+CFUN?', b'invalid'])
                self.output = []

            def __aiter__(self):
                return self

            async def __anext__(self):
                try:
                    return next(self.input)
                except StopIteration:
                    raise StopAsyncIteration

            async def send(self, value):
                self.output.append(value)

        socket = WebSocket()
        with patch('asyncio.create_subprocess_exec', side_effect=create_process):
            await module['handle_client'](socket)
        assert socket.output[0] == 'pong'
        assert json.loads(socket.output[1]) == {'success': True, 'data': '+CFUN: 1', 'error': None, 'command': 'AT+CFUN?'}
        assert json.loads(socket.output[2])['success'] is False
        assert json.loads(socket.output[2])['command'] == ''
        for response, success in [(b'+CMGR: 0\r\nerror in SMS text\r\nOK\r\n', True),
                                  (b'+CMGR: 0\r\nERROR\r\nOK\r\n', True),
                                  (b'+CME ERROR: 50\r\n', False),
                                  (b'+CMS ERROR: 500\r\n', False)]:
            async def response_process(*arguments, **options):
                return Process(response)
            probe = WebSocket()
            probe.input = iter(['ATI'])
            with patch('asyncio.create_subprocess_exec', side_effect=response_process):
                await module['handle_client'](probe)
            assert json.loads(probe.output[0])['success'] is success
        for text in ('ERROR', '+CME ERROR: text'):
            stock.SMSDeliver.decode = lambda stream: {
                'sender': {'number': '12345'}, 'user_data': {'data': text},
                'scts': datetime(2026, 10, 9, 12, 34, 56)}
            async def sms_process(*arguments, **options):
                return Process(b'+CMGR: 0,,23\r\n0011\r\nOK\r\n')
            probe = WebSocket()
            probe.input = iter(['AT+CMGR=1'])
            with patch('asyncio.create_subprocess_exec', side_effect=sms_process):
                await module['handle_client'](probe)
            result = json.loads(probe.output[0])
            assert result['success'] and result['data'].endswith('\n' + text)
        class ClosedWebSocket(WebSocket):
            async def __anext__(self):
                raise ConnectionClosed('1005: browser closed without a status code')
        await module['handle_client'](ClosedWebSocket())

    asyncio.run(fixtures())
    print(json.dumps({'result': 'PASS', 'webui_html_pages': len(html_files),
                      'protocol_cases': 18, 'real_AT_calls': 0,
                      'upstream_version': provenance['version'], 'upstream_commit': provenance['commit']}))


if __name__ == '__main__':
    main()
