"""Adapt the upstream MT5700 WebUI to C8 modem responses."""
from pathlib import Path


NDIS_CONNECTED = r'''function c8NdisConnected(response) {
    return ja(response, "^NDISSTATQRY").some(line => {
        const fields = line.replace(/"/g, "").split(",").map(value => value.trim().toUpperCase());
        const isFamily = value => value === "IPV4" || value === "IPV6";
        const explicitCid = isFamily(fields[4]);
        if (explicitCid && fields[0] !== "8") return false;
        const start = explicitCid ? 1 : 0;
        return [start, start + 4].some(index => fields[index] === "1" && isFamily(fields[index + 3]));
    });
}
'''

NR_SIGNAL = r'''async function c8ReadNrSignal(signal) {
    if (signal.mode !== "NR") return signal;
    const response = await Kt().sendCommand("AT^MONSC");
    if (!response.success || !response.data) return signal;
    const cell = FU(response.data);
    if (cell?.sysMode !== "NR" || !Number.isFinite(cell.rscp) || cell.rscp >= 0) return signal;
    return {
        ...signal,
        rsrp: cell.rscp,
        rsrq: Number.isFinite(cell.ecio) ? cell.ecio : signal.rsrq,
        sinr: Number.isFinite(cell.sinr) ? cell.sinr : signal.sinr,
        signalPercent: ab(cell.rscp)
    };
}

let c8LastNrRefresh = 0;
function c8RefreshNrSignal(refresh) {
    const now = Date.now();
    if (now - c8LastNrRefresh < 5000) return;
    c8LastNrRefresh = now;
    refresh().catch(() => {});
}
'''


CID8_TELEMETRY = r'''const c8WebuiDataCid = 8;
function c8Row(response, prefix) {
    return ja(response, prefix)
        .map(line => line.split(","))
        .find(fields => Number(fields[0]) === c8WebuiDataCid);
}

async function c8ReadAmbr(setRates, setApn) {
    const response = await Kt().sendCommand("AT^DSAMBR=8");
    const fields = response.success && response.data ? c8Row(response.data, "^DSAMBR") : null;
    const rate = value => /^\d+$/.test(value?.trim()) ? Number(value) : null;
    setRates({down: rate(fields?.[1]), up: rate(fields?.[2])});
    const apn = fields?.[3]?.trim().replace(/^"|"$/g, "") || "";
    if (apn) setApn(apn);
    return Boolean(apn);
}

async function c8ReadApn(setApn) {
    const response = await Kt().sendCommand("AT+CGDCONT?");
    const fields = response.success && response.data ? c8Row(response.data, "+CGDCONT") : null;
    const apn = fields?.[2]?.trim().replace(/^"|"$/g, "") || "";
    setApn(fields ? apn || "运营商默认" : "—");
}

async function c8ReadQci(setQci) {
    const response = await Kt().sendCommand("AT+CGEQOSRDP=8");
    const fields = response.success && response.data ? c8Row(response.data, "+CGEQOSRDP") : null;
    const qci = Number(fields?.[1]);
    setQci(jU(Number.isInteger(qci) && qci > 0 && qci <= 255 ? String(qci) : ""));
}
'''


def install(root):
    for bundle in (Path(root) / "www/5700/assets").glob("index-*.js"):
        original = bundle.read_text(encoding="utf-8")
        if "function c8NdisConnected(" in original:
            continue
        if "AT^NDISSTATQRY?" not in original or "function FU(" not in original:
            continue
        updated = original.replace(
            "UK=s=>/\\^NDISSTATQRY:\\s*1\\s*,/i.test(s.replace(/\\r/g,\"\")),U0=",
            "UK=c8NdisConnected,U0=",
            1,
        ).replace(
            'ecio:parseFloat(((A=t[9])==null?void 0:A.trim())||"0")}:n==="WCDMA"',
            'ecio:parseFloat(((A=t[9])==null?void 0:A.trim())||"0"),sinr:parseFloat(t[10])}:n==="WCDMA"',
            1,
        ).replace(
            "const ve=$U(pe.data);ve.mode&&",
            "const ve=await c8ReadNrSignal($U(pe.data));ve.mode&&",
            1,
        ).replace(
            'const{networkMode:Ve,rsrp:nt,rsrq:ht,sinr:Ot,rssi:it}=ke.parsed;let Zn=0',
            'const{networkMode:Ve,rsrp:nt,rsrq:ht,sinr:Ot,rssi:it}=ke.parsed;if(Ve==="NR"){c8RefreshNrSignal(dt);return}let Zn=0',
            1,
        )
        start = updated.find("st=async(pe=!1)=>{if(!pe&&$e.current!==null)")
        end = updated.find("k=async()=>{const pe=(Ot,it=!1)", start)
        if start >= 0 and end > start:
            updated = (updated[:start] + "Y=()=>c8ReadAmbr(w,p),me=()=>c8ReadApn(p),"
                       "Oe=()=>c8ReadQci(C)," + updated[end:])
        updated = updated.replace(
            'if(ke.type==="DSAMBR"&&ke.parsed){',
            'if(ke.type==="DSAMBR"&&ke.parsed?.cid===c8WebuiDataCid){',
            1,
        ).replace(
            'value:YP(_.down??S.down)',
            'value:YP(_.down)',
            1,
        ).replace(
            'value:YP(_.up??S.up)',
            'value:YP(_.up)',
            1,
        ).replace(
            '_.down==null&&_.up==null&&(S.down!=null||S.up!=null)?y.jsx(Fe.Text,{type:"tertiary",size:"small",children:"签约速率取自当前承载的最大速率（^DHCP），^DSAMBR 仅 LTE 模式可查"}):null,',
            "",
            1,
        )
        if ("UK=c8NdisConnected" in updated and "await c8ReadNrSignal" in updated
                and "Oe=()=>c8ReadQci(C)" in updated):
            bundle.write_text(NDIS_CONNECTED + NR_SIGNAL + CID8_TELEMETRY + updated, encoding="utf-8")
