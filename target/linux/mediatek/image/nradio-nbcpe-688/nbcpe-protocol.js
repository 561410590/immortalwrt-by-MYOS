/* MT5700 V2.2.2 NBCPE adapter. AT manual 2024-05-17, pp. 178, 287-309, 388-420. */
(function (root) {
  'use strict';
  function payload(reply, prefix) {
    if (!reply || !reply.success || typeof reply.data !== 'string') return [];
    return reply.data.split(/\r?\n/).map(function (line) { return line.trim(); })
      .filter(function (line) { return line.indexOf(prefix + ':') === 0; })
      .map(function (line) { return line.slice(prefix.length + 1).trim(); });
  }
  function csv(line) {
    return (line.match(/(?:"[^"]*"|[^,])*(?:,|$)/g) || [])
      .filter(function (value) { return value !== ''; })
      .map(function (value) { return value.replace(/,$/, '').trim().replace(/^"|"$/g, ''); });
  }
  function ambr(reply) {
    var line = payload(reply, '^DSAMBR')[0];
    if (!line) return null;
    var values = csv(line), cid = Number(values[0]), down = Number(values[1]), up = Number(values[2]);
    if (values.length !== 3 || cid !== 1 || !Number.isFinite(down) || !Number.isFinite(up) || down < 0 || up < 0) return null;
    return {cid: cid, down: down, up: up};
  }
  function contexts(reply) {
    return payload(reply, '^DCONNSTAT').map(function (line) {
      var values = csv(line);
      return {cid: Number(values[0]), apn: values[1] || '', ipv4: Number(values[2]) === 1,
        ipv6: Number(values[3]) === 1, type: Number(values[4]), ethernet: Number(values[5]) === 1};
    }).filter(function (row) { return row.cid >= 1 && row.cid <= 11; });
  }
  async function connection(service) {
    var rows = contexts(await service.sendCommand('AT^DCONNSTAT?'));
    var active = rows.find(function (row) { return row.cid === 8; }) ||
      {cid: 8, apn: '', ipv4: false, ipv6: false, ethernet: false};
    api.activeCid = active.cid;
    return active;
  }
  function hex(value) {
    var text = String(value).trim().replace(/^0x/i, '');
    if (!/^[0-9a-f]+$/i.test(text)) return '未知';
    if (typeof BigInt === 'function') return BigInt('0x' + text).toString(10);
    var number = parseInt(text, 16);
    return Number.isSafeInteger(number) ? String(number) : '0x' + text;
  }
  function measure(value, kind) {
    if (value === '' || value == null) return null;
    var number = Number(value), limits = {rsrp: [-156, -31, -1256], rsrq: [-43, 20, -348], sinr: [-23, 40, -188]}[kind];
    if (!limits || !Number.isFinite(number) || number === limits[2]) return null;
    if (number >= limits[0] && number <= limits[1]) return number;
    number /= 8;
    return number >= limits[0] && number <= limits[1] ? number : null;
  }
  function acceptEnvelope(adapter, message) {
    var result;
    try { result = JSON.parse(message); } catch (error) { return false; }
    if (!result || typeof result.command !== 'string' || typeof result.success !== 'boolean') return false;
    if (adapter.smsCollecting && result.command.trim() === 'AT+CMGL=4') {
      clearTimeout(adapter.smsTimer);
      if (adapter.smsResolve) adapter.smsResolve(result);
      adapter.smsCollecting = false; adapter.smsResolve = null; adapter.smsTimer = null; adapter.smsBuffer = [];
      return true;
    }
    if (result.command.trim() !== String(adapter.lastCommand || '').trim()) return true;
    if (adapter.pendingCommands && adapter.pendingCommands.size) adapter.handleResponse(result);
    return true;
  }
  async function resolveScs(service, cell, suggestion) {
    var values = payload(await service.sendCommand('AT^MONSC'), '^MONSC').map(csv);
    var current = values.find(function (row) {
      return row[0] === 'NR' && Number(row[3]) === Number(cell.arfcn) && parseInt(row[6], 16) === Number(cell.pci);
    });
    if (current && /^[0-4]$/.test(current[4])) return Number(current[4]);
    var choice = root.prompt('目标邻区未提供 SCS。选择子载波间隔：0=15、1=30、2=60、3=120、4=240 kHz。', String(suggestion));
    if (choice === null) return null;
    if (!/^[0-4]$/.test(choice.trim())) throw new Error('SCS 必须为 0–4');
    return Number(choice.trim());
  }
  function verifyCellLock(reply, cell) {
    var prefix = cell.type === 'NR' ? '^NRFREQLOCK' : '^LTEFREQLOCK';
    if (!reply || !reply.success || !reply.data) return false;
    var lines = reply.data.trim().split(/\r?\n/).map(function (line) { return line.trim(); });
    var header = lines.findIndex(function (line) { return line.indexOf(prefix + ':') === 0; });
    if (header < 0 || Number(lines[header].slice(prefix.length + 1).trim()) !== 2) return false;
    return lines.slice(header + 2).some(function (line) {
      var row = csv(line);
      return Number(row[0]) === Number(cell.band) && Number(row[1]) === Number(cell.arfcn) &&
        (cell.type === 'NR' ? Number(row[2]) === Number(cell.scs) && Number(row[3]) === Number(cell.pci) : Number(row[2]) === Number(cell.pci));
    });
  }
  var api = {payload: payload, csv: csv, ambr: ambr, contexts: contexts, connection: connection,
    hex: hex, measure: measure, acceptEnvelope: acceptEnvelope, resolveScs: resolveScs, verifyCellLock: verifyCellLock, activeCid: 8};
  root.NBCPE = api;
  if (typeof module === 'object' && module.exports) module.exports = api;
})(typeof window === 'object' ? window : globalThis);
