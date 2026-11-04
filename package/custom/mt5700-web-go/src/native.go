package main

import (
	"context"
	"encoding/json"
	"fmt"
	"net"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"time"
)

// The factory cpedata page exposes per-slot monthly/total accounting. Keep
// accounting in the existing WebUI process; never invoke combo or SIM controls.
const cellularTrafficFile = "/mnt/mmcblk0p10/traffic/nradio.json"

type cellularTrafficRow struct {
	Channel string `json:"channel"`
	Slot int `json:"slot"`
	Name string `json:"name"`
	Active bool `json:"active"`
	Monthly uint64 `json:"monthly"`
	Total uint64 `json:"total"`
	Day int `json:"day"`
	Kind string `json:"kind"`
	LimitGB float64 `json:"limit_gb"`
	End string `json:"end"`
	Period string `json:"period"`
	Expired bool `json:"expired"`
}

type cellularTrafficPoint struct {
	Slot int `json:"slot"`
	Boot string `json:"boot"`
	RX uint64 `json:"rx"`
	TX uint64 `json:"tx"`
}

type cellularTrafficStore struct {
	Rows map[string]*cellularTrafficRow `json:"rows"`
	Last map[string]cellularTrafficPoint `json:"last"`
	Started int64 `json:"started"`
	Updated int64 `json:"updated"`
}

var cellularTraffic = struct {
	sync.Mutex
	Data cellularTrafficStore
	Error string
}{Data: cellularTrafficStore{Rows: map[string]*cellularTrafficRow{}, Last: map[string]cellularTrafficPoint{}}}

func cellularCommand(name string, args ...string) []byte {
	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()
	b, _ := exec.CommandContext(ctx, name, args...).Output()
	return b
}

func cellularRouterTime() time.Time {
	z := strings.TrimSpace(string(cellularCommand("date", "+%z")))
	if len(z) == 5 {
		h, _ := strconv.Atoi(z[1:3]); m, _ := strconv.Atoi(z[3:5])
		offset := (h*60+m)*60
		if z[0] == '-' { offset = -offset }
		return time.Now().In(time.FixedZone("router", offset))
	}
	return time.Now()
}

func cellularTrafficPeriod(row *cellularTrafficRow, now time.Time) {
	if row.Day < 1 || row.Day > 28 { row.Day = 1 }
	start := time.Date(now.Year(), now.Month(), row.Day, 0, 0, 0, 0, now.Location())
	if now.Before(start) { start = start.AddDate(0, -1, 0) }
	period := start.Format("2006-01-02")
	if row.Period != period { row.Monthly = 0; row.Period = period }
	if row.Kind == "total" && row.End != "" && !row.Expired {
		end, err := time.ParseInLocation("2006-01-02T15:04", row.End, now.Location())
		if err == nil && !now.Before(end) { row.Total = 0; row.Expired = true }
	}
}

func cellularTrafficSample() {
	var cfg = map[string]string{}
	for _, line := range strings.Split(string(cellularCommand("uci", "-q", "show", "cpesel")), "\n") {
		key, value, found := strings.Cut(line, "=")
		if found { cfg[key] = strings.Trim(value, "'\"") }
	}
	var dump struct { Interfaces []struct {
		Up bool `json:"up"`
		Device string `json:"l3_device"`
		Addresses []json.RawMessage `json:"ipv4-address"`
	} `json:"interface"` }
	_ = json.Unmarshal(cellularCommand("ubus", "call", "network.interface", "dump"), &dump)
	up := map[string]bool{}
	for _, in := range dump.Interfaces { if in.Up && len(in.Addresses) > 0 { up[in.Device] = true } }
	var cached struct { Result []struct { Status struct { Name string `json:"name"`; SIM string `json:"simno"` } `json:"status"` } `json:"result"` }
	cacheBytes, _ := os.ReadFile("/tmp/infocd/cache/cpestatus")
	_ = json.Unmarshal(cacheBytes, &cached)
	actualSIM := map[string]int{}
	for _, item := range cached.Result { slot,_ := strconv.Atoi(item.Status.SIM); if slot > 0 { actualSIM[item.Status.Name] = slot } }
	boot, _ := os.ReadFile("/proc/sys/kernel/random/boot_id")
	now := cellularRouterTime()
	cellularTraffic.Lock()
	defer cellularTraffic.Unlock()
	d := &cellularTraffic.Data
	if d.Started == 0 { d.Started = now.Unix() }
	for _, row := range d.Rows { row.Active = false }
	for _, link := range []struct{ channel, selector, dev, label string }{
		{"cpe", "sim", "eth1", "内置5G"}, {"cpe1", "sim1", "port5", "副5G"},
	} {
		prefix := "cpesel."+link.selector+"."
		max, _ := strconv.Atoi(cfg[prefix+"max"])
		cur, _ := strconv.Atoi(cfg[prefix+"cur"])
		if actualSIM[link.channel] > 0 { cur = actualSIM[link.channel] }
		types := strings.Split(cfg[prefix+"stype"],",")
		physical, embedded := 0,0
		for slot := 1; slot <= max; slot++ {
			key := fmt.Sprintf("%s_%d", link.channel, slot)
			row := d.Rows[key]
			if row == nil { row = &cellularTrafficRow{Channel:link.channel, Slot:slot, Name:fmt.Sprintf("%s · SIM%d",link.label,slot), Day:1, Kind:"monthly"}; d.Rows[key] = row }
			if slot <= len(types) && types[slot-1] == "4" { embedded++; row.Name = fmt.Sprintf("%s · 内置卡%d",link.label,embedded)
			} else { physical++; row.Name = fmt.Sprintf("%s · SIM%d",link.label,physical) }
			row.Active = up[link.dev] && slot == cur
			cellularTrafficPeriod(row, now)
		}
		readCounter := func(name string) uint64 { b,_ := os.ReadFile("/sys/class/net/"+link.dev+"/statistics/"+name); v,_ := strconv.ParseUint(strings.TrimSpace(string(b)),10,64); return v }
		point := cellularTrafficPoint{Slot:cur, Boot:strings.TrimSpace(string(boot)), RX:readCounter("rx_bytes"), TX:readCounter("tx_bytes")}
		last, exists := d.Last[link.channel]
		row := d.Rows[fmt.Sprintf("%s_%d", link.channel,cur)]
		if exists && row != nil && row.Active && last.Slot == cur {
			delta := func(v, old uint64) uint64 { if point.Boot != last.Boot || v < old { return v }; return v-old }
			n := delta(point.RX,last.RX)+delta(point.TX,last.TX)
			row.Monthly += n; row.Total += n
		}
		d.Last[link.channel] = point
	}
	d.Updated = now.Unix()
}

func cellularTrafficSave() error {
	// Caller holds the mutex. The temporary file is replaced, never retained as a backup.
	b, err := json.Marshal(cellularTraffic.Data)
	if err == nil { err = os.MkdirAll(filepath.Dir(cellularTrafficFile),0755) }
	if err == nil { err = os.WriteFile(cellularTrafficFile+".tmp", b,0644) }
	if err == nil { err = os.Rename(cellularTrafficFile+".tmp",cellularTrafficFile) }
	cellularTraffic.Error = ""
	if err != nil { cellularTraffic.Error = err.Error() }
	return err
}

func startCellularTraffic(ctx context.Context) {
	if b, err := os.ReadFile(cellularTrafficFile); err == nil { _ = json.Unmarshal(b, &cellularTraffic.Data) }
	if cellularTraffic.Data.Rows == nil { cellularTraffic.Data.Rows = map[string]*cellularTrafficRow{} }
	if cellularTraffic.Data.Last == nil { cellularTraffic.Data.Last = map[string]cellularTrafficPoint{} }
	cellularTrafficSample()
	cellularTraffic.Lock(); _ = cellularTrafficSave(); cellularTraffic.Unlock()
	go func() {
		ticker := time.NewTicker(15*time.Second); defer ticker.Stop()
		n := 0
		for { select {
		case <-ctx.Done():
			cellularTrafficSample(); cellularTraffic.Lock(); _ = cellularTrafficSave(); cellularTraffic.Unlock(); return
		case <-ticker.C:
			cellularTrafficSample(); n++
			if n%4 == 0 { cellularTraffic.Lock(); _ = cellularTrafficSave(); cellularTraffic.Unlock() }
		} }
	}()
}

func serveCellularTraffic(w http.ResponseWriter, r *http.Request) {
	host,_,_ := net.SplitHostPort(r.RemoteAddr)
	if ip := net.ParseIP(host); ip == nil || !ip.IsLoopback() { http.NotFound(w,r); return }
	cellularTraffic.Lock()
	defer cellularTraffic.Unlock()
	w.Header().Set("Content-Type","application/json; charset=utf-8")
	w.Header().Set("Cache-Control","no-store")
	if r.Method == http.MethodPost {
		var input struct { Action, Key, Kind, End, Reset string; Day int; LimitGB float64 `json:"limit_gb"` }
		if err := json.NewDecoder(r.Body).Decode(&input); err != nil { http.Error(w,err.Error(),400);return }
		row := cellularTraffic.Data.Rows[input.Key]
		if row == nil { http.Error(w,"找不到该卡槽",400);return }
		if input.Action == "save" {
			if input.Day < 1 || input.Day > 28 || input.LimitGB < 0 || (input.Kind != "monthly" && input.Kind != "total") { http.Error(w,"账单参数不合法",400);return }
			if input.End != "" { if _,err := time.Parse("2006-01-02T15:04",input.End);err != nil { http.Error(w,"到期时间不合法",400);return } }
			row.Day, row.Kind, row.LimitGB = input.Day,input.Kind,input.LimitGB
			if row.End != input.End { row.End = input.End; row.Expired = false }
		} else if input.Action == "reset" {
			if input.Reset == "monthly" { row.Monthly = 0 } else if input.Reset == "total" { row.Total = 0 } else { http.Error(w,"清零类型不合法",400);return }
		} else { http.Error(w,"未知操作",400);return }
		if err := cellularTrafficSave(); err != nil { http.Error(w,err.Error(),500);return }
	}
	_ = json.NewEncoder(w).Encode(map[string]any{"ok":true,"data":cellularTraffic.Data,"error":cellularTraffic.Error})
}

const modemSwitchScript = `<script>
(function(){
 const bar=document.createElement('nav');bar.id='c5800-modem-switch';
 bar.style.cssText='position:fixed;top:12px;left:50%;transform:translateX(-50%);z-index:10000;display:flex;gap:4px;padding:4px;background:var(--semi-color-bg-1,#fff);border:1px solid var(--semi-color-border,#ddd);border-radius:8px;font:14px sans-serif';
 [['主模组 · 内置5G','/modem/internal'],['副模组 · PoE 5G','/modem/external']].forEach(([name,path])=>{const link=document.createElement('a');link.textContent=name;link.href=path+'/5700/';link.style.cssText='padding:5px 12px;text-decoration:none;border-radius:5px;color:inherit';if(location.pathname.startsWith(path+'/')){link.style.background='#c7000b';link.style.color='#fff'}bar.appendChild(link)});
 document.body.appendChild(bar);
})();
</script>`
