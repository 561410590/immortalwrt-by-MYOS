package main

import (
	"bufio"
	"context"
	"crypto/sha256"
	"encoding/binary"
	"encoding/json"
	"io"
	"net"
	"net/http"
	"net/http/httptest"
	"net/url"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

type testBackend struct{}

func (testBackend) Execute(_ context.Context, command string) (string, error) {
	return command + "\r\n+CSQ: 23,99\r\nOK", nil
}

func (testBackend) Close() error   { return nil }
func (testBackend) Target() string { return "test-at" }

func testApp(t *testing.T) *webApp {
	t.Helper()
	return &webApp{
		config:  serverConfig{ID: "test", Name: "Test modem", UIVariant: "legacy", Listen: "127.0.0.1:9010", Transport: "serial", Timeout: 4e9},
		hub:     newHub(4),
		backend: testBackend{},
	}
}

func TestModernUIIsDefaultAndEmbedded(t *testing.T) {
	if got := normalizeUIVariant(""); got != "modern" {
		t.Fatalf("empty UI selection = %q, want modern", got)
	}
	if got := normalizeUIVariant("legacy"); got != "legacy" {
		t.Fatalf("legacy UI selection = %q", got)
	}

	app := testApp(t)
	app.config.UIVariant = "modern"
	app.config.BasePath = "/modem/internal"
	server := httptest.NewServer(app)
	defer server.Close()

	response, err := http.Get(server.URL + "/5700/")
	if err != nil {
		t.Fatal(err)
	}
	body, _ := io.ReadAll(response.Body)
	_ = response.Body.Close()
	if response.StatusCode != http.StatusOK || !strings.Contains(string(body), `./assets/index-`) {
		t.Fatalf("modern UI response = %d, %s", response.StatusCode, body)
	}

	response, err = http.Get(server.URL + "/5700/config.json")
	if err != nil {
		t.Fatal(err)
	}
	body, _ = io.ReadAll(response.Body)
	_ = response.Body.Close()
	if !strings.Contains(string(body), `"cellscan_async":false`) {
		t.Fatalf("config does not disable unsupported asynchronous scan: %s", body)
	}

	client := &http.Client{CheckRedirect: func(_ *http.Request, _ []*http.Request) error {
		return http.ErrUseLastResponse
	}}
	response, err = client.Get(server.URL + "/5700/network/info/")
	if err != nil {
		t.Fatal(err)
	}
	_ = response.Body.Close()
	if response.StatusCode != http.StatusTemporaryRedirect || response.Header.Get("Location") != "/modem/internal/5700/#/network/info" {
		t.Fatalf("legacy bookmark redirect = %d, %q", response.StatusCode, response.Header.Get("Location"))
	}

	entries, err := embeddedWeb.ReadDir("web-modern/assets")
	if err != nil {
		t.Fatal(err)
	}
	var bundle []byte
	for _, entry := range entries {
		if strings.HasSuffix(entry.Name(), ".js") {
			bundle, err = embeddedWeb.ReadFile("web-modern/assets/" + entry.Name())
			if err != nil {
				t.Fatal(err)
			}
			break
		}
	}
	for _, marker := range []string{"AT^DSFLOWQRY", "at-ws-info", "ws_url"} {
		if !strings.Contains(string(bundle), marker) {
			t.Fatalf("modern UI bundle is missing %q", marker)
		}
	}
}

func TestHTTPRoutes(t *testing.T) {
	server := httptest.NewServer(testApp(t))
	defer server.Close()
	for _, path := range []string{"/healthz", "/cgi-bin/at-ws-info", "/5700/", "/scripts/loading.js"} {
		response, err := http.Get(server.URL + path)
		if err != nil {
			t.Fatalf("GET %s: %v", path, err)
		}
		_, _ = io.Copy(io.Discard, response.Body)
		_ = response.Body.Close()
		if response.StatusCode != http.StatusOK {
			t.Fatalf("GET %s returned %d", path, response.StatusCode)
		}
	}
}

func TestPasswordAuthentication(t *testing.T) {
	sum := sha256.Sum256([]byte("router-secret"))
	auth := authConfig{Enabled: true, Username: "admin", PasswordHash: sum[:]}

	unauthorized := httptest.NewRecorder()
	if auth.require(unauthorized, httptest.NewRequest(http.MethodGet, "/", nil)) {
		t.Fatal("request without credentials was accepted")
	}
	if unauthorized.Code != http.StatusUnauthorized {
		t.Fatalf("unauthorized status = %d", unauthorized.Code)
	}

	request := httptest.NewRequest(http.MethodGet, "/", nil)
	request.SetBasicAuth("admin", "router-secret")
	if !auth.require(httptest.NewRecorder(), request) {
		t.Fatal("valid credentials were rejected")
	}
	request.SetBasicAuth("admin", "wrong")
	if auth.require(httptest.NewRecorder(), request) {
		t.Fatal("invalid password was accepted")
	}
}

func TestInstanceAssetRewrite(t *testing.T) {
	rewritten := string(rewriteWebAsset("index.html", []byte(
		`<link href="/5700/app.css"><script src="/scripts/loading.js"></script>`), "/modem/ak68"))
	for _, expected := range []string{
		`/modem/ak68/5700/app.css`, `/modem/ak68/scripts/loading.js?v=c2000max-mt5700-r6`,
	} {
		if !strings.Contains(rewritten, expected) {
			t.Fatalf("rewritten asset is missing %q: %s", expected, rewritten)
		}
	}
	shim := string(rewriteWebAsset("scripts/loading.js", []byte("ready();"), "/modem/ak68"))
	if !strings.Contains(shim, `/modem/ak68`) || !strings.Contains(shim, `RoutedWebSocket`) ||
		!strings.Contains(shim, `__c2000maxAsyncAssetCacheV6`) ||
		!strings.Contains(shim, `asset.searchParams.set('c2000max','mt5700-r6')`) {
		t.Fatalf("WebSocket routing shim is missing: %s", shim)
	}
	rootHTML := string(rewriteWebAsset("index.html", []byte(`<script src="/scripts/loading.js"></script>`), ""))
	if !strings.Contains(rootHTML, `/scripts/loading.js?v=c2000max-mt5700-r6`) {
		t.Fatalf("root instance loading script is not cache busted: %s", rootHTML)
	}
	rootShim := string(rewriteWebAsset("scripts/loading.js", []byte("ready();"), ""))
	if !strings.Contains(rootShim, `__c2000maxAsyncAssetCacheV6`) {
		t.Fatal("root instance does not install the async chunk cache buster")
	}
}

func TestWebSocketStabilityPatch(t *testing.T) {
	asset, err := embeddedWeb.ReadFile("web/umi.ec9b4b52.js")
	if err != nil {
		t.Fatal(err)
	}
	content := string(rewriteWebAsset("umi.ec9b4b52.js", asset, "/modem/internal"))
	if !strings.Contains(content, `I()(this,"maxReconnectAttempts",30)`) {
		t.Fatal("vendor WebSocket reconnect budget was not increased")
	}
	if strings.Contains(content, `I()(this,"maxReconnectAttempts",3)`) {
		t.Fatal("three-attempt WebSocket reconnect limit is still active")
	}
	html, err := embeddedWeb.ReadFile("web/index.html")
	if err != nil {
		t.Fatal(err)
	}
	rewrittenHTML := string(rewriteWebAsset("index.html", html, "/modem/internal"))
	if !strings.Contains(rewrittenHTML, "umi.ec9b4b52.js?v=c2000max-mt5700-r7") {
		t.Fatal("updated main bundle is not cache busted")
	}
	if websocketPingInterval > 20*time.Second || websocketWriteTimeout > 10*time.Second {
		t.Fatal("WebSocket keepalive or write timeout is too relaxed")
	}
}

func TestNetworkSpeedAssetUsesDSFlowFallback(t *testing.T) {
	asset, err := embeddedWeb.ReadFile("web/p__CPE__Network__Info__index.30901ff8.async.js")
	if err != nil {
		t.Fatal(err)
	}
	content := string(rewriteWebAsset(networkInfoAsset, asset, "/modem/internal"))
	if !strings.HasPrefix(content, `(function(base){`) {
		t.Fatal("standalone DS flow poller is not executed before the vendor chunk")
	}
	for _, expected := range []string{
		`__c2000maxDsflowSpeedV3`,
		`new window.WebSocket`,
		`/modem/internal`,
		`socket.send('AT^DSFLOWQRY')`,
		`setInterval(tick,500)`,
		`Object.prototype.hasOwnProperty.call(payload,'success')`,
		`data-c2000max-dsflow`,
		`BigInt('0x'+m[5])`,
		`C2kSpeedSampleRef`,
		`C2kSpeedSampleRef.current.lastUpdateTime`,
		`E.sendCommand("AT^DSFLOWQRY")`,
		`PDCP \u4E0A\u62A5\u4E0D\u53EF\u7528`,
		`an(_e.upSpeed)`,
		`an(_e.downSpeed)`,
	} {
		if !strings.Contains(content, expected) {
			t.Fatalf("network-speed fallback is missing %q", expected)
		}
	}
	if strings.Contains(content, `se?an(se.ulPdcpRate):"0 bps"`) ||
		strings.Contains(content, `se?an(se.dlPdcpRate):"0 bps"`) {
		t.Fatal("real-time speed still falls back to a zero PDCP value")
	}
	if strings.Contains(content, `Ce("networkSpeed",!0,1)`) {
		t.Fatal("real-time speed still relies on the unrelated one-second scheduler")
	}
	if strings.Contains(content, `C2kSpeedPollRef`) {
		t.Fatal("network-speed fallback still relies on a minified React polling hook")
	}
}

func TestNetworkSpeedAssetDisablesBrowserCaching(t *testing.T) {
	server := httptest.NewServer(testApp(t))
	defer server.Close()
	response, err := http.Get(server.URL + "/5700/" + networkInfoAsset)
	if err != nil {
		t.Fatal(err)
	}
	defer response.Body.Close()
	if cacheControl := response.Header.Get("Cache-Control"); cacheControl != "no-store" {
		t.Fatalf("network-speed asset cache policy = %q", cacheControl)
	}
}

func TestMultiInstanceRoutes(t *testing.T) {
	app := testApp(t)
	app.config.ID = "ak68"
	app.config.Name = "AK68 聚合模组"
	app.config.BasePath = "/modem/ak68"
	multi := &multiWebApp{
		instances: []*webApp{app},
		byPath:    map[string]*webApp{"/modem/ak68": app},
	}
	server := httptest.NewServer(multi)
	defer server.Close()

	response, err := http.Get(server.URL + "/")
	if err != nil {
		t.Fatal(err)
	}
	body, _ := io.ReadAll(response.Body)
	_ = response.Body.Close()
	if response.StatusCode != http.StatusOK || !strings.Contains(string(body), "/modem/ak68/5700/") {
		t.Fatalf("dashboard response = %d, %s", response.StatusCode, body)
	}

	response, err = http.Get(server.URL + "/modem/ak68/5700/")
	if err != nil {
		t.Fatal(err)
	}
	body, _ = io.ReadAll(response.Body)
	_ = response.Body.Close()
	if response.StatusCode != http.StatusOK || !strings.Contains(string(body), "/modem/ak68/5700/umi.31851818.css") {
		t.Fatalf("instance page response = %d", response.StatusCode)
	}
}

func TestInstancePathValidation(t *testing.T) {
	for _, value := range []string{"internal", "ak68-2", "modem_3"} {
		if !validInstancePath(value) {
			t.Fatalf("valid instance path %q was rejected", value)
		}
	}
	for _, value := range []string{"", "AK68", "../modem", "-modem", strings.Repeat("a", 33)} {
		if validInstancePath(value) {
			t.Fatalf("invalid instance path %q was accepted", value)
		}
	}
}

func TestDiscoveryMatchesExplicitPortToModem(t *testing.T) {
	dir := t.TempDir()
	uci := filepath.Join(dir, "uci")
	script := `#!/bin/sh
cat <<'EOF'
qmodem.first.name='Internal MT5700'
qmodem.first.model='MT5700M-CN'
qmodem.first.at_port='/dev/ttyUSB1'
qmodem.second.name='AK68 MT5700'
qmodem.second.model='MT5700M-CN'
qmodem.second.at_port='/dev/ttyUSB9'
EOF
`
	if err := os.WriteFile(uci, []byte(script), 0755); err != nil {
		t.Fatal(err)
	}
	t.Setenv("PATH", dir+string(os.PathListSeparator)+os.Getenv("PATH"))
	discovery := discoverModems("auto", "/dev/ttyUSB9")
	if discovery.SelectedSection != "second" || discovery.SelectedPort != "/dev/ttyUSB9" || discovery.SelectedModel != "MT5700M-CN" {
		t.Fatalf("explicit port selected wrong modem: %+v", discovery)
	}
}

func writeMaskedText(t *testing.T, conn net.Conn, text string) {
	t.Helper()
	payload := []byte(text)
	header := []byte{0x81}
	switch {
	case len(payload) < 126:
		header = append(header, 0x80|byte(len(payload)))
	case len(payload) <= 65535:
		header = append(header, 0x80|126, 0, 0)
		binary.BigEndian.PutUint16(header[len(header)-2:], uint16(len(payload)))
	default:
		t.Fatal("test payload too large")
	}
	mask := []byte{1, 2, 3, 4}
	header = append(header, mask...)
	for index := range payload {
		payload[index] ^= mask[index%4]
	}
	if _, err := conn.Write(append(header, payload...)); err != nil {
		t.Fatal(err)
	}
}

func readServerText(t *testing.T, reader *bufio.Reader) string {
	t.Helper()
	header := make([]byte, 2)
	if _, err := io.ReadFull(reader, header); err != nil {
		t.Fatal(err)
	}
	length := uint64(header[1] & 0x7f)
	if length == 126 {
		extra := make([]byte, 2)
		if _, err := io.ReadFull(reader, extra); err != nil {
			t.Fatal(err)
		}
		length = uint64(binary.BigEndian.Uint16(extra))
	} else if length == 127 {
		extra := make([]byte, 8)
		if _, err := io.ReadFull(reader, extra); err != nil {
			t.Fatal(err)
		}
		length = binary.BigEndian.Uint64(extra)
	}
	payload := make([]byte, length)
	if _, err := io.ReadFull(reader, payload); err != nil {
		t.Fatal(err)
	}
	return string(payload)
}

func TestWebSocketATProtocol(t *testing.T) {
	server := httptest.NewServer(testApp(t))
	defer server.Close()
	parsed, _ := url.Parse(server.URL)
	conn, err := net.Dial("tcp", parsed.Host)
	if err != nil {
		t.Fatal(err)
	}
	defer conn.Close()
	request := "GET / HTTP/1.1\r\nHost: " + parsed.Host + "\r\n" +
		"Upgrade: websocket\r\nConnection: Upgrade\r\n" +
		"Sec-WebSocket-Version: 13\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n"
	if _, err := conn.Write([]byte(request)); err != nil {
		t.Fatal(err)
	}
	reader := bufio.NewReader(conn)
	status, err := reader.ReadString('\n')
	if err != nil || !strings.Contains(status, "101") {
		t.Fatalf("handshake failed: %q, %v", status, err)
	}
	for {
		line, err := reader.ReadString('\n')
		if err != nil {
			t.Fatal(err)
		}
		if line == "\r\n" {
			break
		}
	}

	writeMaskedText(t, conn, "ping")
	if response := readServerText(t, reader); response != "pong" {
		t.Fatalf("unexpected heartbeat response: %q", response)
	}
	writeMaskedText(t, conn, "AT+CSQ")
	var response atResponse
	if err := json.Unmarshal([]byte(readServerText(t, reader)), &response); err != nil {
		t.Fatal(err)
	}
	if !response.Success || !strings.Contains(response.Data, "+CSQ: 23,99") {
		t.Fatalf("unexpected AT response: %+v", response)
	}
}

func TestATResponseDetection(t *testing.T) {
	if !finalATResponse([]byte("AT+CSQ\r\n+CSQ: 20,99\r\nOK\r\n")) {
		t.Fatal("OK response not detected")
	}
	if finalATResponse([]byte("+CSQ: 20,99\r\n")) {
		t.Fatal("partial response detected as complete")
	}
	cleaned := cleanATResponse("AT+CSQ", []byte("AT+CSQ\r\n+CSQ: 20,99\r\nOK\r\n"))
	if strings.Contains(cleaned, "AT+CSQ") || !strings.Contains(cleaned, "+CSQ") {
		t.Fatalf("unexpected cleaned response: %q", cleaned)
	}
}

func TestQModemQueuedBackend(t *testing.T) {
	dir := t.TempDir()
	helper := filepath.Join(dir, "qmodem-at")
	script := "#!/bin/sh\nprintf '%s\\r\\n+CSQ: 31,99\\r\\nOK\\r\\n' \"$3\"\n"
	if err := os.WriteFile(helper, []byte(script), 0755); err != nil {
		t.Fatal(err)
	}
	backend := newQModemQueuedBackend("auto", "/dev/mock-at", helper, 2*time.Second, 10*time.Second)
	defer backend.Close()
	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()
	response, err := backend.Execute(ctx, "AT+CSQ")
	if err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(response, "+CSQ: 31,99") || strings.Contains(response, "AT+CSQ") {
		t.Fatalf("unexpected queued response: %q", response)
	}
	if !strings.Contains(backend.Target(), "QModem") {
		t.Fatalf("queue target is not exposed: %q", backend.Target())
	}
}

func TestQModemBackendCloseStopsActiveHelper(t *testing.T) {
	dir := t.TempDir()
	helper := filepath.Join(dir, "qmodem-at")
	started := filepath.Join(dir, "started")
	script := "#!/bin/sh\necho started > \"$STARTED_FILE\"\ntrap 'exit 0' TERM INT\nsleep 30\n"
	if err := os.WriteFile(helper, []byte(script), 0755); err != nil {
		t.Fatal(err)
	}
	t.Setenv("STARTED_FILE", started)
	backend := newQModemQueuedBackend("auto", "/dev/mock-at", helper, time.Second, time.Second)
	done := make(chan error, 1)
	go func() {
		_, err := backend.Execute(context.Background(), "AT")
		done <- err
	}()
	deadline := time.Now().Add(3 * time.Second)
	for {
		if _, err := os.Stat(started); err == nil {
			break
		}
		if time.Now().After(deadline) {
			t.Fatal("helper did not start")
		}
		time.Sleep(10 * time.Millisecond)
	}
	start := time.Now()
	if err := backend.Close(); err != nil {
		t.Fatal(err)
	}
	if elapsed := time.Since(start); elapsed > 5*time.Second {
		t.Fatalf("backend close took %s", elapsed)
	}
	select {
	case <-done:
	case <-time.After(time.Second):
		t.Fatal("active helper did not return after close")
	}
}

func TestQModemQueuedBackendInitialDiscoveryTarget(t *testing.T) {
	backend := newQModemQueuedBackend("auto", "auto", "/bin/false", time.Second, time.Second)
	if got := backend.Target(); !strings.Contains(got, "等待发现模组") {
		t.Fatalf("unexpected undiscovered target: %q", got)
	}
	backend.setDiscoveredPort(" /dev/ttyUSB1 ")
	if got := backend.Target(); got != "/dev/ttyUSB1（QModem 已发现，等待 AT 命令）" {
		t.Fatalf("connected modem is still reported as undiscovered: %q", got)
	}
}

func TestLongCommandTimeoutSelection(t *testing.T) {
	regular := 8 * time.Second
	long := 240 * time.Second
	if got := timeoutForATCommand(" AT+COPS=? ", regular, long); got != long {
		t.Fatalf("COPS scan timeout = %s, want %s", got, long)
	}
	if got := timeoutForATCommand("AT+CSQ", regular, long); got != regular {
		t.Fatalf("normal timeout = %s, want %s", got, regular)
	}
}

func TestRotatingLogWriter(t *testing.T) {
	path := filepath.Join(t.TempDir(), "mt5700.log")
	writer, err := openRotatingLog(path, 32, 2)
	if err != nil {
		t.Fatal(err)
	}
	for i := 0; i < 5; i++ {
		if _, err := writer.Write([]byte("0123456789abcdef\n")); err != nil {
			t.Fatal(err)
		}
	}
	if err := writer.Close(); err != nil {
		t.Fatal(err)
	}
	if _, err := os.Stat(path + ".1"); err != nil {
		t.Fatalf("rotated log missing: %v", err)
	}
	if _, err := os.Stat(path + ".3"); !os.IsNotExist(err) {
		t.Fatalf("log backup limit was exceeded: %v", err)
	}
}
