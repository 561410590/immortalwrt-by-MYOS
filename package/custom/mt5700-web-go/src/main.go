package main

import (
	"bufio"
	"context"
	"crypto/sha1"
	"crypto/sha256"
	"crypto/subtle"
	"embed"
	"encoding/base64"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"html/template"
	"io"
	"io/fs"
	"log"
	"mime"
	"net"
	"net/http"
	"net/url"
	"os"
	"os/exec"
	"os/signal"
	"path"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"time"
)

const (
	version               = "3.0.2"
	websocketPingInterval = 20 * time.Second
	websocketWriteTimeout = 10 * time.Second
)

// All modem instances use the V3 interface, including saved legacy settings.
//
//go:embed web-modern
var embeddedWeb embed.FS

type modemInfo struct {
	Section      string `json:"section"`
	Name         string `json:"name"`
	Model        string `json:"model"`
	Manufacturer string `json:"manufacturer"`
	ATPort       string `json:"at_port"`
	Supported    bool   `json:"supported"`
}

type discoveryInfo struct {
	Modems          []modemInfo `json:"modems"`
	Ports           []string    `json:"ports"`
	SelectedSection string      `json:"selected_section"`
	SelectedPort    string      `json:"selected_port"`
	SelectedModel   string      `json:"selected_model"`
	Supported       bool        `json:"supported"`
}

func trimUCIValue(value string) string {
	value = strings.TrimSpace(value)
	if len(value) >= 2 && ((value[0] == '\'' && value[len(value)-1] == '\'') ||
		(value[0] == '"' && value[len(value)-1] == '"')) {
		value = value[1 : len(value)-1]
	}
	return strings.ReplaceAll(value, "'\\''", "'")
}

type uciSection struct {
	Name    string
	Type    string
	Options map[string]string
}

func readUCISections(packageName string) ([]uciSection, error) {
	if packageName == "" {
		return nil, errors.New("UCI package is empty")
	}
	output, err := exec.Command("uci", "-q", "show", packageName).Output()
	if err != nil {
		return nil, err
	}
	sections := make(map[string]*uciSection)
	order := make([]string, 0)
	prefix := packageName + "."
	for _, line := range strings.Split(string(output), "\n") {
		left, value, ok := strings.Cut(line, "=")
		if !ok || !strings.HasPrefix(left, prefix) {
			continue
		}
		remainder := strings.TrimPrefix(left, prefix)
		parts := strings.SplitN(remainder, ".", 2)
		name := parts[0]
		if name == "" {
			continue
		}
		section, exists := sections[name]
		if !exists {
			section = &uciSection{Name: name, Options: make(map[string]string)}
			sections[name] = section
			order = append(order, name)
		}
		if len(parts) == 1 {
			section.Type = trimUCIValue(value)
		} else {
			section.Options[parts[1]] = trimUCIValue(value)
		}
	}
	result := make([]uciSection, 0, len(order))
	for _, name := range order {
		result = append(result, *sections[name])
	}
	return result, nil
}

func optionBool(options map[string]string, name string, fallback bool) bool {
	value, exists := options[name]
	if !exists || value == "" {
		return fallback
	}
	switch strings.ToLower(value) {
	case "1", "true", "yes", "on":
		return true
	case "0", "false", "no", "off":
		return false
	default:
		return fallback
	}
}

func optionInt(options map[string]string, name string, fallback int) int {
	value, err := strconv.Atoi(options[name])
	if err != nil {
		return fallback
	}
	return value
}

func optionDuration(options map[string]string, name string, fallback time.Duration) time.Duration {
	value := strings.TrimSpace(options[name])
	if value == "" {
		return fallback
	}
	parsed, err := time.ParseDuration(value)
	if err != nil || parsed <= 0 {
		return fallback
	}
	return parsed
}

func normalizeUIVariant(value string) string {
	return "modern"
}

func validInstancePath(value string) bool {
	if len(value) < 1 || len(value) > 32 {
		return false
	}
	for index, char := range value {
		if (char >= 'a' && char <= 'z') || (char >= '0' && char <= '9') ||
			(index > 0 && (char == '-' || char == '_')) {
			continue
		}
		return false
	}
	return true
}

func mt5700Model(model, manufacturer string) bool {
	identity := strings.ToLower(model + " " + manufacturer)
	return strings.Contains(identity, "mt5700") ||
		(strings.Contains(identity, "td tech") && strings.Contains(identity, "5700"))
}

func discoverModems(requestedSection, requestedPort string) discoveryInfo {
	result := discoveryInfo{Modems: []modemInfo{}, Ports: []string{}}
	output, _ := exec.Command("uci", "-q", "show", "qmodem").Output()
	sections := make(map[string]map[string]string)
	order := make([]string, 0)
	for _, line := range strings.Split(string(output), "\n") {
		if !strings.HasPrefix(line, "qmodem.") {
			continue
		}
		left, value, ok := strings.Cut(line, "=")
		if !ok {
			continue
		}
		parts := strings.Split(left, ".")
		if len(parts) != 3 {
			continue
		}
		section, option := parts[1], parts[2]
		if _, exists := sections[section]; !exists {
			sections[section] = make(map[string]string)
			order = append(order, section)
		}
		sections[section][option] = trimUCIValue(value)
	}

	for _, section := range order {
		values := sections[section]
		port := values["override_at_port"]
		if port == "" {
			port = values["at_port"]
		}
		if port == "" {
			continue
		}
		model := values["model"]
		if model == "" {
			model = values["modem_name"]
		}
		name := values["name"]
		if model == "" {
			model = name
		}
		if name == "" {
			name = model
		}
		if name == "" {
			name = section
		}
		info := modemInfo{
			Section: section, Name: name, Model: model,
			Manufacturer: values["manufacturer"], ATPort: port,
		}
		info.Supported = mt5700Model(info.Model, info.Manufacturer)
		result.Modems = append(result.Modems, info)
	}

	portSet := make(map[string]struct{})
	for _, modem := range result.Modems {
		if modem.ATPort != "" {
			portSet[modem.ATPort] = struct{}{}
		}
	}
	for _, pattern := range []string{
		"/dev/ttyUSB*", "/dev/ttyACM*", "/dev/mhi_DUN*", "/dev/wwan*at*",
	} {
		matches, _ := filepath.Glob(pattern)
		for _, match := range matches {
			portSet[match] = struct{}{}
		}
	}
	for port := range portSet {
		result.Ports = append(result.Ports, port)
	}
	sort.Strings(result.Ports)

	selectedSection := requestedSection
	if requestedPort != "" && requestedPort != "auto" {
		for _, modem := range result.Modems {
			if modem.ATPort == requestedPort {
				selectedSection = modem.Section
				break
			}
		}
	}
	if selectedSection == "" || selectedSection == "auto" {
		if len(result.Modems) > 0 {
			selectedSection = result.Modems[0].Section
		} else {
			selectedSection = "auto"
		}
	}
	result.SelectedSection = selectedSection
	for _, modem := range result.Modems {
		if modem.Section == selectedSection {
			result.SelectedPort = modem.ATPort
			result.SelectedModel = modem.Model
			if result.SelectedModel == "" {
				result.SelectedModel = modem.Name
			}
			result.Supported = modem.Supported
			break
		}
	}
	if requestedPort != "" && requestedPort != "auto" {
		result.SelectedPort = requestedPort
	}
	if result.SelectedPort == "" && len(result.Ports) > 0 {
		result.SelectedPort = result.Ports[0]
	}
	return result
}

type atBackend interface {
	Execute(context.Context, string) (string, error)
	Close() error
	Target() string
}

type inputFlusher interface {
	FlushInput() error
}

type persistentBackend struct {
	kind        string
	target      string
	timeout     time.Duration
	longTimeout time.Duration
	opener      func() (io.ReadWriteCloser, string, error)
	broadcast   func(string)

	commandMu sync.Mutex
	connMu    sync.Mutex
	conn      io.ReadWriteCloser

	stateMu sync.RWMutex
	active  chan []byte
	closed  bool
}

func (b *persistentBackend) Target() string {
	b.stateMu.RLock()
	defer b.stateMu.RUnlock()
	return b.target
}

func (b *persistentBackend) ensureConnection() (io.ReadWriteCloser, error) {
	b.connMu.Lock()
	defer b.connMu.Unlock()
	if b.closed {
		return nil, errors.New("AT 后端已关闭")
	}
	if b.conn != nil {
		return b.conn, nil
	}
	conn, target, err := b.opener()
	if err != nil {
		return nil, err
	}
	b.conn = conn
	b.stateMu.Lock()
	b.target = target
	b.stateMu.Unlock()
	go b.readLoop(conn)
	log.Printf("AT %s 已连接：%s", b.kind, target)
	return conn, nil
}

func (b *persistentBackend) dropConnection(conn io.ReadWriteCloser) {
	b.connMu.Lock()
	if b.conn == conn {
		_ = b.conn.Close()
		b.conn = nil
	}
	b.connMu.Unlock()
}

func (b *persistentBackend) readLoop(conn io.ReadWriteCloser) {
	buffer := make([]byte, 8192)
	for {
		n, err := conn.Read(buffer)
		if n > 0 {
			chunk := append([]byte(nil), buffer[:n]...)
			b.stateMu.RLock()
			active := b.active
			b.stateMu.RUnlock()
			if active != nil {
				select {
				case active <- chunk:
				case <-time.After(500 * time.Millisecond):
					log.Printf("AT 响应缓冲区已满，丢弃 %d 字节", len(chunk))
				}
			} else if b.broadcast != nil {
				b.broadcast(string(chunk))
			}
		}
		if err != nil {
			if !errors.Is(err, io.EOF) && !errors.Is(err, os.ErrClosed) {
				log.Printf("AT %s 读取失败：%v", b.kind, err)
			}
			b.dropConnection(conn)
			return
		}
		if n == 0 {
			time.Sleep(20 * time.Millisecond)
		}
	}
}

func finalATResponse(data []byte) bool {
	normalized := strings.ReplaceAll(string(data), "\r", "")
	for _, line := range strings.Split(normalized, "\n") {
		line = strings.TrimSpace(line)
		if line == "OK" || line == "ERROR" || line == "NO CARRIER" ||
			strings.HasPrefix(line, "+CME ERROR:") || strings.HasPrefix(line, "+CMS ERROR:") {
			return true
		}
	}
	return false
}

func cleanATResponse(command string, data []byte) string {
	normalized := strings.ReplaceAll(string(data), "\x00", "")
	command = strings.TrimSpace(strings.TrimRight(command, "\r\n"))
	lines := strings.Split(strings.ReplaceAll(normalized, "\r\n", "\n"), "\n")
	filtered := make([]string, 0, len(lines))
	removedEcho := false
	for _, line := range lines {
		if !removedEcho && strings.TrimSpace(line) == command {
			removedEcho = true
			continue
		}
		filtered = append(filtered, strings.TrimRight(line, "\r"))
	}
	return strings.TrimSpace(strings.Join(filtered, "\r\n"))
}

func normalizeATCommand(command string) string {
	command = strings.ReplaceAll(command, "\x00", "")
	command = strings.TrimSpace(strings.TrimRight(command, "\r\n"))
	if strings.HasPrefix(command, "AT^SYSCFGEX") {
		command = strings.ReplaceAll(command, "\n", "")
		command = strings.ReplaceAll(command, "\r", "")
		command = strings.ReplaceAll(command, "OK", "")
	}
	return command
}

func timeoutForATCommand(command string, regular, long time.Duration) time.Duration {
	command = strings.ToUpper(strings.ReplaceAll(strings.TrimSpace(command), " ", ""))
	if command == "AT+COPS=?" && long > regular {
		return long
	}
	return regular
}

func (b *persistentBackend) Execute(ctx context.Context, command string) (string, error) {
	b.commandMu.Lock()
	defer b.commandMu.Unlock()

	command = normalizeATCommand(command)
	if command == "" {
		return "", errors.New("AT 命令为空")
	}
	if len(command) > 64*1024 {
		return "", errors.New("AT 命令过长")
	}
	conn, err := b.ensureConnection()
	if err != nil {
		return "", err
	}
	if flusher, ok := conn.(inputFlusher); ok {
		_ = flusher.FlushInput()
	}

	responseCh := make(chan []byte, 64)
	b.stateMu.Lock()
	b.active = responseCh
	b.stateMu.Unlock()
	defer func() {
		b.stateMu.Lock()
		if b.active == responseCh {
			b.active = nil
		}
		b.stateMu.Unlock()
	}()

	wireCommand := command
	if !strings.HasSuffix(wireCommand, "\r") {
		wireCommand += "\r"
	}
	if _, err := conn.Write([]byte(wireCommand)); err != nil {
		b.dropConnection(conn)
		return "", fmt.Errorf("写入 AT 端口失败：%w", err)
	}

	timeout := timeoutForATCommand(command, b.timeout, b.longTimeout)
	if timeout <= 0 {
		timeout = 8 * time.Second
	}
	timer := time.NewTimer(timeout)
	defer timer.Stop()
	var response []byte
	for {
		select {
		case chunk := <-responseCh:
			response = append(response, chunk...)
			if len(response) > 2*1024*1024 {
				return "", errors.New("AT 响应超过 2 MiB 限制")
			}
			if finalATResponse(response) {
				return cleanATResponse(command, response), nil
			}
		case <-timer.C:
			if len(response) > 0 {
				return cleanATResponse(command, response), fmt.Errorf("AT 响应超时（收到不完整数据）")
			}
			return "", errors.New("AT 响应超时")
		case <-ctx.Done():
			return "", ctx.Err()
		}
	}
}

func (b *persistentBackend) Close() error {
	b.connMu.Lock()
	defer b.connMu.Unlock()
	b.closed = true
	if b.conn != nil {
		err := b.conn.Close()
		b.conn = nil
		return err
	}
	return nil
}

type qmodemQueuedBackend struct {
	section        string
	configuredPort string
	helper         string
	timeout        time.Duration
	longTimeout    time.Duration
	gate           chan struct{}
	closeCh        chan struct{}

	stateMu sync.RWMutex
	target  string
	closed  bool
	active  sync.WaitGroup
}

func newQModemQueuedBackend(section, configuredPort, helper string, timeout, longTimeout time.Duration) *qmodemQueuedBackend {
	return &qmodemQueuedBackend{
		section: section, configuredPort: configuredPort, helper: helper,
		timeout: timeout, longTimeout: longTimeout, gate: make(chan struct{}, 1),
		closeCh: make(chan struct{}),
	}
}

func (b *qmodemQueuedBackend) Target() string {
	if b.configuredPort == "cpe1" {
		return "cpe1（AK原厂API）"
	}
	b.stateMu.RLock()
	defer b.stateMu.RUnlock()
	if b.target == "" {
		return "原厂共享AT通道"
	}
	return b.target
}

func (b *qmodemQueuedBackend) setDiscoveredPort(port string) {
	port = strings.TrimSpace(port)
	if port == "" {
		return
	}
	b.stateMu.Lock()
	b.target = port + "（原厂共享AT通道）"
	b.stateMu.Unlock()
}

func (b *qmodemQueuedBackend) Execute(ctx context.Context, command string) (string, error) {
	command = normalizeATCommand(command)
	if command == "" {
		return "", errors.New("AT 命令为空")
	}
	if len(command) > 64*1024 {
		return "", errors.New("AT 命令过长")
	}
	select {
	case b.gate <- struct{}{}:
		defer func() { <-b.gate }()
	case <-ctx.Done():
		return "", fmt.Errorf("等待 5700 面板 AT 队列超时：%w", ctx.Err())
	case <-b.closeCh:
		return "", errors.New("AT 后端已关闭")
	}

	b.stateMu.Lock()
	if b.closed {
		b.stateMu.Unlock()
		return "", errors.New("AT 后端已关闭")
	}
	b.active.Add(1)
	b.stateMu.Unlock()
	defer b.active.Done()
	discovery := discoverModems(b.section, b.configuredPort)
	port := discovery.SelectedPort
	if b.configuredPort != "" && b.configuredPort != "auto" {
		port = b.configuredPort
	}
	if port == "" {
		return "", errors.New("QModem 尚未发现可用 AT 串口，请在 LuCI 中手动选择")
	}
	b.stateMu.Lock()
	b.target = port + "（原厂共享AT通道）"
	b.stateMu.Unlock()

	timeout := timeoutForATCommand(command, b.timeout, b.longTimeout)
	seconds := int((timeout + time.Second - 1) / time.Second)
	if seconds < 1 {
		seconds = 1
	}
	commandCtx, cancel := context.WithCancel(ctx)
	closedWatchDone := make(chan struct{})
	go func() {
		select {
		case <-b.closeCh:
			cancel()
		case <-closedWatchDone:
		}
	}()
	defer func() {
		close(closedWatchDone)
		cancel()
	}()
	cmd := exec.CommandContext(commandCtx, b.helper, port, strconv.Itoa(seconds), command)
	// The helper contains lock-owning shell children. Terminate the process
	// group so procd restart cannot leave a queued AT request running behind the
	// new panel process; shell EXIT traps still get SIGTERM and release locks.
	cmd.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	cmd.Cancel = func() error {
		if cmd.Process == nil {
			return os.ErrProcessDone
		}
		return syscall.Kill(-cmd.Process.Pid, syscall.SIGTERM)
	}
	cmd.WaitDelay = 3 * time.Second
	output, err := cmd.CombinedOutput()
	if len(output) > 2*1024*1024 {
		return "", errors.New("AT 响应超过 2 MiB 限制")
	}
	cleaned := cleanATResponse(command, output)
	if err != nil {
		// Modem-level ERROR/CME/CMS is a valid completed transaction and is
		// reported to the browser as an AT error, not a broken queue.
		if finalATResponse(output) {
			return cleaned, nil
		}
		if cleaned != "" {
			return "", fmt.Errorf("QModem AT 队列失败：%w：%s", err, cleaned)
		}
		return "", fmt.Errorf("QModem AT 队列失败：%w", err)
	}
	return cleaned, nil
}

func (b *qmodemQueuedBackend) Close() error {
	b.stateMu.Lock()
	if !b.closed {
		b.closed = true
		close(b.closeCh)
	}
	b.stateMu.Unlock()
	b.active.Wait()
	return nil
}

func newNetworkBackend(host string, port int, timeout, longTimeout time.Duration, broadcast func(string)) *persistentBackend {
	target := net.JoinHostPort(host, strconv.Itoa(port))
	backend := &persistentBackend{kind: "网络", target: target, timeout: timeout, longTimeout: longTimeout, broadcast: broadcast}
	backend.opener = func() (io.ReadWriteCloser, string, error) {
		conn, err := net.DialTimeout("tcp", target, timeout)
		if err != nil {
			return nil, target, fmt.Errorf("连接网络 AT %s 失败：%w", target, err)
		}
		return conn, target, nil
	}
	return backend
}

type wsClient struct {
	conn net.Conn
	rw   *bufio.ReadWriter
	mu   sync.Mutex
}

func (c *wsClient) writeFrame(opcode byte, payload []byte) error {
	c.mu.Lock()
	defer c.mu.Unlock()
	if err := c.conn.SetWriteDeadline(time.Now().Add(websocketWriteTimeout)); err != nil {
		return err
	}
	defer c.conn.SetWriteDeadline(time.Time{})
	header := []byte{0x80 | opcode}
	switch {
	case len(payload) < 126:
		header = append(header, byte(len(payload)))
	case len(payload) <= 65535:
		header = append(header, 126, 0, 0)
		binary.BigEndian.PutUint16(header[len(header)-2:], uint16(len(payload)))
	default:
		header = append(header, 127, 0, 0, 0, 0, 0, 0, 0, 0)
		binary.BigEndian.PutUint64(header[len(header)-8:], uint64(len(payload)))
	}
	if _, err := c.rw.Write(header); err != nil {
		return err
	}
	if _, err := c.rw.Write(payload); err != nil {
		return err
	}
	return c.rw.Flush()
}

func (c *wsClient) writeText(text string) error {
	return c.writeFrame(0x1, []byte(text))
}

func (c *wsClient) readFrame() (bool, byte, []byte, error) {
	header := make([]byte, 2)
	if _, err := io.ReadFull(c.rw, header); err != nil {
		return false, 0, nil, err
	}
	fin := header[0]&0x80 != 0
	opcode := header[0] & 0x0f
	masked := header[1]&0x80 != 0
	length := uint64(header[1] & 0x7f)
	if length == 126 {
		extra := make([]byte, 2)
		if _, err := io.ReadFull(c.rw, extra); err != nil {
			return false, 0, nil, err
		}
		length = uint64(binary.BigEndian.Uint16(extra))
	} else if length == 127 {
		extra := make([]byte, 8)
		if _, err := io.ReadFull(c.rw, extra); err != nil {
			return false, 0, nil, err
		}
		length = binary.BigEndian.Uint64(extra)
	}
	if !masked {
		return false, 0, nil, errors.New("客户端 WebSocket 帧未掩码")
	}
	if length > 2*1024*1024 {
		return false, 0, nil, errors.New("WebSocket 消息超过 2 MiB 限制")
	}
	mask := make([]byte, 4)
	if _, err := io.ReadFull(c.rw, mask); err != nil {
		return false, 0, nil, err
	}
	payload := make([]byte, int(length))
	if _, err := io.ReadFull(c.rw, payload); err != nil {
		return false, 0, nil, err
	}
	for index := range payload {
		payload[index] ^= mask[index%4]
	}
	return fin, opcode, payload, nil
}

type wsHub struct {
	mu      sync.RWMutex
	clients map[*wsClient]struct{}
	limit   int
}

func newHub(limit int) *wsHub {
	return &wsHub{clients: make(map[*wsClient]struct{}), limit: limit}
}

func (h *wsHub) add(client *wsClient) bool {
	h.mu.Lock()
	defer h.mu.Unlock()
	if h.limit > 0 && len(h.clients) >= h.limit {
		return false
	}
	h.clients[client] = struct{}{}
	return true
}

func (h *wsHub) remove(client *wsClient) {
	h.mu.Lock()
	delete(h.clients, client)
	h.mu.Unlock()
	_ = client.conn.Close()
}

func (h *wsHub) count() int {
	h.mu.RLock()
	defer h.mu.RUnlock()
	return len(h.clients)
}

func (h *wsHub) broadcastRaw(data string) {
	data = strings.TrimSpace(strings.ReplaceAll(data, "\x00", ""))
	if data == "" {
		return
	}
	payload, _ := json.Marshal(map[string]any{"type": "raw_data", "data": data})
	h.mu.RLock()
	clients := make([]*wsClient, 0, len(h.clients))
	for client := range h.clients {
		clients = append(clients, client)
	}
	h.mu.RUnlock()
	for _, client := range clients {
		if err := client.writeText(string(payload)); err != nil {
			h.remove(client)
		}
	}
}

type serverConfig struct {
	ID            string
	Name          string
	BasePath      string
	UIVariant     string
	Listen        string
	Transport     string
	Section       string
	SerialPort    string
	Baud          int
	NetworkHost   string
	NetworkPort   int
	Timeout       time.Duration
	LongTimeout   time.Duration
	QueueTimeout  time.Duration
	WSIdleTimeout time.Duration
	MaxClients    int
	LogFile       string
	Model         string
	Manufacturer  string
}

type webApp struct {
	config  serverConfig
	hub     *wsHub
	backend atBackend
}

type authConfig struct {
	Enabled      bool
	Username     string
	PasswordHash []byte
}

type multiWebApp struct {
	listen    string
	auth      authConfig
	instances []*webApp
	byPath    map[string]*webApp
}

func parsePasswordHash(value string) ([]byte, error) {
	value = strings.TrimSpace(value)
	if value == "" {
		return nil, nil
	}
	decoded, err := hex.DecodeString(value)
	if err != nil || len(decoded) != sha256.Size {
		return nil, errors.New("password hash must be a 64-character SHA-256 hex digest")
	}
	return decoded, nil
}

func (a authConfig) verify(username, password string) bool {
	if !a.Enabled {
		return true
	}
	if len(a.PasswordHash) != sha256.Size {
		return false
	}
	userOK := subtle.ConstantTimeCompare([]byte(username), []byte(a.Username)) == 1
	sum := sha256.Sum256([]byte(password))
	passwordOK := subtle.ConstantTimeCompare(sum[:], a.PasswordHash) == 1
	return userOK && passwordOK
}

func (a authConfig) require(w http.ResponseWriter, r *http.Request) bool {
	if !a.Enabled {
		return true
	}
	username, password, ok := r.BasicAuth()
	if ok && a.verify(username, password) {
		return true
	}
	w.Header().Set("WWW-Authenticate", `Basic realm="MT5700 Control Panel", charset="UTF-8"`)
	w.Header().Set("Cache-Control", "no-store")
	http.Error(w, "需要输入 MT5700 控制面板用户名和密码", http.StatusUnauthorized)
	return false
}

func websocketRequest(r *http.Request) bool {
	return strings.EqualFold(r.Header.Get("Upgrade"), "websocket") &&
		strings.Contains(strings.ToLower(r.Header.Get("Connection")), "upgrade")
}

func sameOrigin(r *http.Request) bool {
	origin := r.Header.Get("Origin")
	if origin == "" {
		return true
	}
	parsed, err := url.Parse(origin)
	if err != nil {
		return false
	}
	originHost := strings.Trim(strings.ToLower(parsed.Hostname()), "[]")
	requestHost := r.Host
	if host, _, err := net.SplitHostPort(r.Host); err == nil {
		requestHost = host
	}
	requestHost = strings.Trim(strings.ToLower(requestHost), "[]")
	return originHost == requestHost
}

func websocketAccept(key string) string {
	digest := sha1.Sum([]byte(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"))
	return base64.StdEncoding.EncodeToString(digest[:])
}

func (a *webApp) upgradeWebSocket(w http.ResponseWriter, r *http.Request) {
	if !sameOrigin(r) {
		http.Error(w, "WebSocket Origin 不匹配", http.StatusForbidden)
		return
	}
	key := strings.TrimSpace(r.Header.Get("Sec-WebSocket-Key"))
	if key == "" || r.Header.Get("Sec-WebSocket-Version") != "13" {
		http.Error(w, "无效的 WebSocket 握手", http.StatusBadRequest)
		return
	}
	hijacker, ok := w.(http.Hijacker)
	if !ok {
		http.Error(w, "当前 HTTP 服务不支持 WebSocket", http.StatusInternalServerError)
		return
	}
	conn, rw, err := hijacker.Hijack()
	if err != nil {
		return
	}
	client := &wsClient{conn: conn, rw: rw}
	if !a.hub.add(client) {
		_, _ = rw.WriteString("HTTP/1.1 503 Service Unavailable\r\nConnection: close\r\n\r\n")
		_ = rw.Flush()
		_ = conn.Close()
		return
	}
	_, _ = rw.WriteString("HTTP/1.1 101 Switching Protocols\r\n")
	_, _ = rw.WriteString("Upgrade: websocket\r\n")
	_, _ = rw.WriteString("Connection: Upgrade\r\n")
	_, _ = rw.WriteString("Sec-WebSocket-Accept: " + websocketAccept(key) + "\r\n\r\n")
	if err := rw.Flush(); err != nil {
		a.hub.remove(client)
		return
	}
	go a.serveWebSocket(client)
}

type atResponse struct {
	Success bool   `json:"success"`
	Data    string `json:"data,omitempty"`
	Error   string `json:"error,omitempty"`
}

func atResponseIsError(response string) bool {
	upper := strings.ToUpper(response)
	return strings.Contains(upper, "\r\nERROR") || strings.HasPrefix(upper, "ERROR") ||
		strings.Contains(upper, "+CME ERROR:") || strings.Contains(upper, "+CMS ERROR:") ||
		strings.Contains(upper, "NO CARRIER")
}

func (a *webApp) commandTimeout(command string) time.Duration {
	return timeoutForATCommand(command, a.config.Timeout, a.config.LongTimeout)
}

func (a *webApp) handleATCommand(client *wsClient, command string) {
	if command == "ping" {
		_ = client.writeText("pong")
		return
	}
	command = normalizeATCommand(command)
	response := atResponse{}
	if command == "AT+CONNECT?" {
		kind := "1"
		if a.config.Transport == "network" {
			kind = "0"
		}
		response.Success = true
		response.Data = "+CONNECT: " + kind + "\r\nOK"
	} else {
		deadline := a.commandTimeout(command) + a.config.QueueTimeout + 2*time.Second
		ctx, cancel := context.WithTimeout(context.Background(), deadline)
		data, err := a.backend.Execute(ctx, command)
		cancel()
		if err != nil {
			response.Error = err.Error()
		} else if atResponseIsError(data) {
			response.Error = data
		} else {
			response.Success = true
			response.Data = data
		}
	}
	payload, _ := json.Marshal(response)
	_ = client.writeText(string(payload))
}

func (a *webApp) serveWebSocket(client *wsClient) {
	defer a.hub.remove(client)
	keepaliveDone := make(chan struct{})
	defer close(keepaliveDone)
	go func() {
		ticker := time.NewTicker(websocketPingInterval)
		defer ticker.Stop()
		for {
			select {
			case <-ticker.C:
				if err := client.writeFrame(0x9, []byte("c2000max")); err != nil {
					_ = client.conn.Close()
					return
				}
			case <-keepaliveDone:
				return
			}
		}
	}()
	var fragmented []byte
	var fragmentOpcode byte
	for {
		if a.config.WSIdleTimeout > 0 {
			_ = client.conn.SetReadDeadline(time.Now().Add(a.config.WSIdleTimeout))
		}
		fin, opcode, payload, err := client.readFrame()
		if err != nil {
			return
		}
		switch opcode {
		case 0x8:
			_ = client.writeFrame(0x8, payload)
			return
		case 0x9:
			_ = client.writeFrame(0xA, payload)
			continue
		case 0xA:
			continue
		case 0x1, 0x2:
			if fin {
				if opcode == 0x1 {
					a.handleATCommand(client, string(payload))
				}
				continue
			}
			fragmentOpcode = opcode
			fragmented = append(fragmented[:0], payload...)
		case 0x0:
			fragmented = append(fragmented, payload...)
			if len(fragmented) > 2*1024*1024 {
				return
			}
			if fin {
				if fragmentOpcode == 0x1 {
					a.handleATCommand(client, string(fragmented))
				}
				fragmented = nil
				fragmentOpcode = 0
			}
		default:
			return
		}
	}
}

func hostWithoutPort(hostport string) string {
	if host, _, err := net.SplitHostPort(hostport); err == nil {
		return strings.Trim(host, "[]")
	}
	if strings.HasPrefix(hostport, "[") && strings.Contains(hostport, "]") {
		return strings.Trim(strings.Split(hostport, "]")[0], "[]")
	}
	if strings.Count(hostport, ":") == 1 {
		return strings.Split(hostport, ":")[0]
	}
	return strings.Trim(hostport, "[]")
}

func listenPort(listen string) int {
	_, port, err := net.SplitHostPort(listen)
	if err != nil {
		return 9010
	}
	value, err := strconv.Atoi(port)
	if err != nil {
		return 9010
	}
	return value
}

func (a *webApp) wsInfo(w http.ResponseWriter, r *http.Request) {
	host := hostWithoutPort(r.Host)
	port := listenPort(a.config.Listen)
	response := map[string]any{
		"success": true,
		"data": map[string]any{
			"host": host, "port": port, "allow_wan": 0,
			"require_auth": false,
			"capabilities": map[string]bool{
				"cellscan_async":  false,
				"schedule_config": false,
			},
			"ws_url":    strings.Replace(requestBaseURL(r), "http", "ws", 1) + a.config.BasePath + "/ws",
			"instance":  a.config.ID,
			"timestamp": time.Now().Unix(),
		},
	}
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.Header().Set("Access-Control-Allow-Origin", "*")
	w.Header().Set("Cache-Control", "no-store")
	_ = json.NewEncoder(w).Encode(response)
}

func (a *webApp) configJSON(w http.ResponseWriter, r *http.Request) {
	host := hostWithoutPort(r.Host)
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.Header().Set("Cache-Control", "no-store")
	_ = json.NewEncoder(w).Encode(map[string]any{
		"at":     map[string]any{"host": host, "port": listenPort(a.config.Listen)},
		"status": "true", "require_auth": false, "instance": a.config.ID,
		"capabilities": map[string]bool{
			"cellscan_async":  false,
			"schedule_config": false,
		},
		"ui_variant": a.config.UIVariant,
		"ws_url":     strings.Replace(requestBaseURL(r), "http", "ws", 1) + a.config.BasePath + "/ws",
	})
}

func (a *webApp) health(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.Header().Set("Cache-Control", "no-store")
	_ = json.NewEncoder(w).Encode(map[string]any{
		"ok": true, "service": "mt5700-web-go", "version": version,
		"id": a.config.ID, "name": a.config.Name, "path": a.config.BasePath,
		"ui_variant": a.config.UIVariant,
		"transport":  a.config.Transport, "target": a.backend.Target(),
		"model": a.config.Model, "manufacturer": a.config.Manufacturer,
		"supported":         mt5700Model(a.config.Model, a.config.Manufacturer),
		"websocket_clients": a.hub.count(),
	})
}

func browserRouteShim(basePath string) string {
	encoded, _ := json.Marshal(basePath)
	return `(function(base){
if(!window.__c2000maxAsyncAssetCacheV6){
 window.__c2000maxAsyncAssetCacheV6=true;
 var NativeAppendChild=window.Element.prototype.appendChild;
 window.Element.prototype.appendChild=function(node){
  if(node&&node.tagName==='SCRIPT'&&node.src){
   try{
    var asset=new URL(node.src,window.location.href);
    if(asset.host===window.location.host&&/\.async\.js$/.test(asset.pathname)){
     asset.searchParams.set('c2000max','mt5700-r6');
     node.src=asset.toString();
    }
   }catch(e){}
  }
  return NativeAppendChild.call(this,node);
 };
}
var NativeWebSocket=window.WebSocket;
function RoutedWebSocket(url,protocols){
 try {
  var parsed=new URL(url,window.location.href);
  if(parsed.host===window.location.host&&(parsed.pathname==="/"||parsed.pathname==="")){
   parsed.pathname=base+"/ws";
   url=parsed.toString();
  }
 } catch(e) {}
 return arguments.length>1?new NativeWebSocket(url,protocols):new NativeWebSocket(url);
}
RoutedWebSocket.prototype=NativeWebSocket.prototype;
["CONNECTING","OPEN","CLOSING","CLOSED"].forEach(function(name){
 Object.defineProperty(RoutedWebSocket,name,{value:NativeWebSocket[name]});
});
window.WebSocket=RoutedWebSocket;
})(` + string(encoded) + `);
`
}

// networkSpeedCompatibilityShim is deliberately independent from the bundled
// React component.  The vendor page has changed its internal hook layout more
// than once, making minified-symbol patches unreliable on real browsers.  This
// small client owns a separate WebSocket, samples the documented cumulative DS
// counters every 500 ms while the speed switch is enabled and updates the two
// Ant Design Statistic values in place.
func networkSpeedCompatibilityShim(basePath string) string {
	encoded, _ := json.Marshal(basePath)
	return `(function(base){
if(window.__c2000maxDsflowSpeedV3)return;
window.__c2000maxDsflowSpeedV3=true;
var socket=null,busy=false,last=null,idleTicks=0;
function findStatistic(title){
 var nodes=document.querySelectorAll('.ant-statistic');
 for(var i=0;i<nodes.length;i++){
  var label=nodes[i].querySelector('.ant-statistic-title');
  if(label&&label.textContent.trim()===title)return nodes[i];
 }
 return null;
}
function targets(){
 var up=findStatistic('上行速率'),down=findStatistic('下行速率');
 return up&&down?{up:up,down:down}:null;
}
function enabled(){
	var t=targets(),scope=t&&t.up;
	for(var depth=0;scope&&depth<8;depth++,scope=scope.parentElement){
	 if(scope.textContent.indexOf('实时网速')<0)continue;
	 var node=scope.querySelector('button[role="switch"],.ant-switch');
	 if(node)return node.getAttribute('aria-checked')==='true'||node.classList.contains('ant-switch-checked');
	}
	return true;
}
function setValue(stat,value){
 var node=stat.querySelector('.ant-statistic-content-value')||stat.querySelector('.ant-statistic-content');
 if(node){node.textContent=value;node.setAttribute('data-c2000max-dsflow','1');}
}
function format(bytesPerSecond){
 var bits=Math.max(0,bytesPerSecond*8);
 if(bits>=1e9)return(bits/1e9).toFixed(2)+' Gbps';
 if(bits>=1e6)return(bits/1e6).toFixed(2)+' Mbps';
 if(bits>=1e3)return(bits/1e3).toFixed(2)+' Kbps';
 return Math.round(bits)+' bps';
}
function render(up,down){
 var t=targets();
 if(!t)return;
 setValue(t.up,format(up));setValue(t.down,format(down));
}
function consume(raw){
	var payload;
	try{payload=JSON.parse(raw);}catch(e){return;}
	if(!payload||!Object.prototype.hasOwnProperty.call(payload,'success'))return;
	busy=false;
	if(!payload.success||typeof payload.data!=='string')return;
 var compact=payload.data.replace(/\s+/g,'');
 var m=compact.match(/\^DSFLOWQRY:([0-9A-Fa-f]{1,8}),([0-9A-Fa-f]{1,16}),([0-9A-Fa-f]{1,16}),([0-9A-Fa-f]{1,8}),([0-9A-Fa-f]{1,16}),([0-9A-Fa-f]{1,16})/);
 if(!m)return;
 var now=performance.now(),tx=BigInt('0x'+m[5]),rx=BigInt('0x'+m[6]);
 if(last){
  var seconds=(now-last.time)/1000,txDelta=tx-last.tx,rxDelta=rx-last.rx;
  render(txDelta>=0&&seconds>0?Number(txDelta)/seconds:0,rxDelta>=0&&seconds>0?Number(rxDelta)/seconds:0);
 }
 last={time:now,tx:tx,rx:rx};
}
function closeSocket(){
 if(socket){try{socket.close();}catch(e){}socket=null;}
 busy=false;last=null;
}
function ensureSocket(){
 if(socket&&(socket.readyState===0||socket.readyState===1))return;
 var scheme=location.protocol==='https:'?'wss://':'ws://';
 socket=new window.WebSocket(scheme+location.host+base+'/ws');
 socket.onmessage=function(event){consume(event.data);};
 socket.onclose=function(){socket=null;busy=false;last=null;};
 socket.onerror=function(){busy=false;};
}
function tick(){
 var t=targets();
 if(!t){if(++idleTicks>10)closeSocket();return;}
 idleTicks=0;
 if(!enabled()){closeSocket();render(0,0);return;}
 ensureSocket();
 if(socket&&socket.readyState===1&&!busy){busy=true;socket.send('AT^DSFLOWQRY');}
}
window.__c2000maxDsflowTimer=setInterval(tick,500);tick();
})(` + string(encoded) + `);
`
}

const networkInfoAsset = "p__CPE__Network__Info__index.30901ff8.async.js"

// The supplied panel bundle calculates a DSFLOWQRY byte-counter delta but
// never renders it: the real-time cards only consult PDCP push reports, which
// remain zero on MT5700M-CN firmware.  Keep the proprietary bundle intact on
// disk and apply a small, regression-tested compatibility patch while serving
// this one asset.
func patchNetworkInfoAsset(name string, data []byte) []byte {
	if name != networkInfoAsset {
		return data
	}
	content := string(data)
	replacements := [][2]string{
		{
			`vr=R()(La,2),_e=vr[0],br=vr[1],Ze=`,
			`vr=R()(La,2),_e=vr[0],br=vr[1],C2kSpeedSampleRef=(0,j.useRef)({lastUpdateTime:0,lastTxFlow:0,lastRxFlow:0}),Ze=`,
		},
		{
			`_e.lastUpdateTime>0?(t=(r-_e.lastUpdateTime)/1e3,t>0&&(g=u-_e.lastTxFlow,v=s-_e.lastRxFlow,h=g/t,x=v/t,br({upSpeed:h,downSpeed:x,lastUpdateTime:r,lastTxFlow:u,lastRxFlow:s}))):br(o()(o()({},_e),{},{lastUpdateTime:r,lastTxFlow:u,lastRxFlow:s})),Oa(`,
			`C2kSpeedSampleRef.current.lastUpdateTime>0?(t=(r-C2kSpeedSampleRef.current.lastUpdateTime)/1e3,t>0&&(g=u-C2kSpeedSampleRef.current.lastTxFlow,v=s-C2kSpeedSampleRef.current.lastRxFlow,h=g>=0?g/t:0,x=v>=0?v/t:0,br({upSpeed:h,downSpeed:x,lastUpdateTime:r,lastTxFlow:u,lastRxFlow:s}))):br({upSpeed:0,downSpeed:0,lastUpdateTime:r,lastTxFlow:u,lastRxFlow:s}),C2kSpeedSampleRef.current={lastUpdateTime:r,lastTxFlow:u,lastRxFlow:s},Oa(`,
		},
		{
			`):ne.ZP.error("\u5B9E\u65F6\u7F51\u901F\u5F00\u542F\u5931\u8D25"),a.next=10`,
			`):(Ve(!0),hn(!1),Xr(xn),ne.ZP.warning("PDCP \u4E0A\u62A5\u4E0D\u53EF\u7528\uFF0C\u5DF2\u6539\u7528 DS \u6D41\u91CF\u91C7\u6837"),mn(!1)),a.next=10`,
		},
		{
			`case 7:a.prev=7,a.t0=a.catch(0),ne.ZP.error("\u8BBE\u7F6EPDCP\u6570\u636E\u4E0A\u62A5\u5931\u8D25")`,
			`case 7:a.prev=7,a.t0=a.catch(0),Ve(!0),hn(!1),Xr(xn),ne.ZP.warning("PDCP \u4E0A\u62A5\u4E0D\u53EF\u7528\uFF0C\u5DF2\u6539\u7528 DS \u6D41\u91CF\u91C7\u6837"),mn(!1)`,
		},
		{
			`case 5:return r.prev=5`,
			`case 5:return C2kSpeedSampleRef.current={lastUpdateTime:0,lastTxFlow:0,lastRxFlow:0},br({upSpeed:0,downSpeed:0,lastUpdateTime:0,lastTxFlow:0,lastRxFlow:0}),r.prev=5`,
		},
		{
			`d=r.sent,d.success?(Ve(!1),fn(null),hn(!1),ne.ZP.success("\u5173\u95ED\u5B9E\u65F6\u7F51\u901F\u6210\u529F")):ne.ZP.error("\u5173\u95ED\u5B9E\u65F6\u7F51\u901F\u5931\u8D25")`,
			`d=r.sent,Ve(!1),fn(null),hn(!1),d.success?ne.ZP.success("\u5173\u95ED\u5B9E\u65F6\u7F51\u901F\u6210\u529F"):ne.ZP.warning("PDCP \u5173\u95ED\u5931\u8D25\uFF0CDS \u6D41\u91CF\u91C7\u6837\u5DF2\u505C\u6B62")`,
		},
		{
			`case 12:r.prev=12,r.t0=r.catch(5),ne.ZP.error("\u8BBE\u7F6EPDCP\u6570\u636E\u4E0A\u62A5\u5931\u8D25")`,
			`case 12:r.prev=12,r.t0=r.catch(5),Ve(!1),fn(null),hn(!1),ne.ZP.warning("PDCP \u5173\u95ED\u5931\u8D25\uFF0CDS \u6D41\u91CF\u91C7\u6837\u5DF2\u505C\u6B62")`,
		},
		{
			`se?an(se.ulPdcpRate):"0 bps"`,
			`se&&se.ulPdcpRate>0?an(se.ulPdcpRate):an(_e.upSpeed)`,
		},
		{
			`se?an(se.dlPdcpRate):"0 bps"`,
			`se&&se.dlPdcpRate>0?an(se.dlPdcpRate):an(_e.downSpeed)`,
		},
		{
			`((re==null?void 0:re.ulPdcpRate)||(se==null?void 0:se.ulPdcpRate)||0)*8`,
			`((re==null?void 0:re.ulPdcpRate)||(se==null?void 0:se.ulPdcpRate)||_e.upSpeed||0)*8`,
		},
		{
			`((re==null?void 0:re.dlPdcpRate)||(se==null?void 0:se.dlPdcpRate)||0)*8`,
			`((re==null?void 0:re.dlPdcpRate)||(se==null?void 0:se.dlPdcpRate)||_e.downSpeed||0)*8`,
		},
	}
	for _, replacement := range replacements {
		content = strings.ReplaceAll(content, replacement[0], replacement[1])
	}
	return []byte(content)
}

func rewriteWebAsset(name string, data []byte, basePath string) []byte {
	data = patchNetworkInfoAsset(name, data)
	if name == networkInfoAsset {
		data = append([]byte(networkSpeedCompatibilityShim(basePath)), data...)
	}
	extension := strings.ToLower(path.Ext(name))
	if extension != ".html" && extension != ".js" && extension != ".css" && extension != ".json" && extension != ".svg" {
		return data
	}
	content := string(data)
	if name == "umi.ec9b4b52.js" {
		content = strings.ReplaceAll(content,
			`I()(this,"maxReconnectAttempts",3)`,
			`I()(this,"maxReconnectAttempts",30)`)
	} else if name == "index.html" {
		content = strings.ReplaceAll(content, "/umi.ec9b4b52.js", "/umi.ec9b4b52.js?v=c2000max-mt5700-r7")
	}
	content = strings.ReplaceAll(content, "/scripts/loading.js", basePath+"/scripts/loading.js?v=c2000max-mt5700-r6")
	if basePath != "" {
		content = strings.ReplaceAll(content, "/5700/", basePath+"/5700/")
		content = strings.ReplaceAll(content, "/cgi-bin/at-ws-info", basePath+"/cgi-bin/at-ws-info")
		content = strings.ReplaceAll(content, "/cgi-bin/at-log-clear", basePath+"/cgi-bin/at-log-clear")
	}
	if name == "scripts/loading.js" {
		content = browserRouteShim(basePath) + content
	}
	return []byte(content)
}

func contentTypeFor(name string) string {
	switch strings.ToLower(path.Ext(name)) {
	case ".js":
		return "application/javascript; charset=utf-8"
	case ".css":
		return "text/css; charset=utf-8"
	case ".html":
		return "text/html; charset=utf-8"
	case ".json":
		return "application/json; charset=utf-8"
	case ".svg":
		return "image/svg+xml"
	}
	return mime.TypeByExtension(path.Ext(name))
}

func (a *webApp) serveWebAsset(w http.ResponseWriter, r *http.Request, name string) {
	name = strings.TrimPrefix(path.Clean("/"+name), "/")
	if name == "." || name == "" || strings.HasSuffix(r.URL.Path, "/") {
		if name == "." || name == "" {
			name = "index.html"
		} else {
			name += "/index.html"
		}
	}
	variant := normalizeUIVariant(a.config.UIVariant)
	assetRoot := "web-modern"
	if variant == "legacy" {
		assetRoot = "web"
	}
	data, err := fs.ReadFile(embeddedWeb, assetRoot+"/"+name)
	if err != nil {
		http.NotFound(w, r)
		return
	}
	if variant == "legacy" {
		data = rewriteWebAsset(name, data, a.config.BasePath)
	}
	if variant == "modern" {
		data = rewriteWebAsset(name, data, a.config.BasePath)
		data = []byte(strings.ReplaceAll(string(data), "V3.0.0", "V3.0.2"))
		if name == "index.html" {
			data = []byte(strings.ReplaceAll(string(data), "</head>", "<script>"+browserRouteShim(a.config.BasePath)+"</script></head>"))
			data = []byte(strings.ReplaceAll(string(data), "</body>", modemSwitchScript+"</body>"))
		}
	}
	if contentType := contentTypeFor(name); contentType != "" {
		w.Header().Set("Content-Type", contentType)
	}
	if strings.HasSuffix(name, ".html") || (variant == "legacy" && (name == networkInfoAsset || name == "scripts/loading.js")) {
		w.Header().Set("Cache-Control", "no-store")
	} else {
		w.Header().Set("Cache-Control", "public, max-age=604800")
	}
	w.Header().Set("Content-Length", strconv.Itoa(len(data)))
	_, _ = w.Write(data)
}

func modernLegacyRedirect(requestPath, basePath string) (string, bool) {
	route := strings.Trim(strings.TrimPrefix(requestPath, "/5700"), "/")
	switch route {
	case "network", "network/info":
		route = "/network/info"
	case "network/setting":
		route = "/network/setting"
	case "network/dial":
		route = "/network/dial"
	case "system", "system/info":
		route = "/system/info"
	case "system/upgrade":
		route = "/system/upgrade"
	case "sms", "sms/center":
		route = "/sms/center"
	case "sms/settings":
		route = "/sms/settings"
	case "at":
		route = "/at"
	default:
		return "", false
	}
	return basePath + "/5700/#" + route, true
}

func (a *webApp) clearLog(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	if a.config.LogFile == "" {
		_ = json.NewEncoder(w).Encode(map[string]any{"success": false, "error": "未配置日志文件"})
		return
	}
	if err := os.Truncate(a.config.LogFile, 0); err != nil {
		_ = json.NewEncoder(w).Encode(map[string]any{"success": false, "error": err.Error()})
		return
	}
	_ = json.NewEncoder(w).Encode(map[string]any{"success": true, "message": "日志已清空"})
}

func (a *webApp) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("X-Content-Type-Options", "nosniff")
	w.Header().Set("Referrer-Policy", "same-origin")
	if a.serveFactoryAPI(w, r) {
		return
	}
	if websocketRequest(r) {
		if r.URL.Path == "/" || r.URL.Path == "/ws" {
			a.upgradeWebSocket(w, r)
		} else {
			http.NotFound(w, r)
		}
		return
	}
	if normalizeUIVariant(a.config.UIVariant) == "modern" {
		if target, ok := modernLegacyRedirect(r.URL.Path, a.config.BasePath); ok {
			http.Redirect(w, r, target, http.StatusTemporaryRedirect)
			return
		}
	}
	switch r.URL.Path {
	case "/":
		http.Redirect(w, r, "/5700/", http.StatusFound)
		return
	case "/cgi-bin/at-ws-info":
		a.wsInfo(w, r)
		return
	case "/cgi-bin/at-log-clear":
		a.clearLog(w, r)
		return
	case "/5700/config.json":
		a.configJSON(w, r)
		return
	case "/healthz":
		a.health(w, r)
		return
	case "/scripts/loading.js":
		a.serveWebAsset(w, r, "scripts/loading.js")
		return
	}
	if strings.HasPrefix(r.URL.Path, "/5700/") {
		a.serveWebAsset(w, r, strings.TrimPrefix(r.URL.Path, "/5700/"))
		return
	}
	http.NotFound(w, r)
}

var dashboardTemplate = template.Must(template.New("dashboard").Parse(`<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>MT5700 模组管理地址</title><style>
:root{color-scheme:light dark;font-family:system-ui,-apple-system,"Segoe UI",sans-serif}body{margin:0;background:#f3f5f7;color:#1f2937}.wrap{max-width:980px;margin:0 auto;padding:32px 18px}.head{margin-bottom:22px}.head h1{margin:0 0 8px;font-size:28px}.head p{margin:0;color:#64748b}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:16px}.card{padding:20px;border:1px solid #dbe3ea;border-radius:14px;background:#fff;box-shadow:0 8px 28px rgba(15,23,42,.06)}.card h2{margin:0 0 8px;font-size:18px}.meta{margin:6px 0;color:#64748b;font-size:14px;overflow-wrap:anywhere}.open{display:inline-block;margin-top:12px;padding:9px 14px;border-radius:8px;background:#168ab0;color:#fff;text-decoration:none;font-weight:700}.empty{padding:24px;border:1px dashed #94a3b8;border-radius:12px}.foot{margin-top:20px;color:#64748b;font-size:13px}@media(prefers-color-scheme:dark){body{background:#0f172a;color:#e5e7eb}.card{background:#111827;border-color:#334155}.head p,.meta,.foot{color:#94a3b8}}
</style></head><body><main class="wrap"><div class="head"><h1>MT5700 模组控制面板</h1><p>请选择要管理的模组；每个路径对应独立的串口或网络 AT 后端。</p></div>
{{if .Instances}}<div class="grid">{{range .Instances}}<section class="card"><h2>{{.Name}}</h2><div class="meta">实例：{{.ID}}</div><div class="meta">界面：{{if eq .UIVariant "legacy"}}旧版兼容界面{{else}}新版 Semi Design 界面{{end}}</div><div class="meta">连接：{{.Transport}} · {{.Target}}</div><div class="meta">地址：{{.URL}}</div><a class="open" href="{{.Path}}/5700/">打开控制面板</a></section>{{end}}</div>{{else}}<div class="empty">当前没有启用的模组实例，请在 LuCI 的“MT5700 控制面板”中添加并启用。</div>{{end}}
<div class="foot">MT5700 Web {{.Version}}</div></main></body></html>`))

type dashboardInstance struct {
	ID        string
	Name      string
	Transport string
	Target    string
	Path      string
	URL       string
	UIVariant string
}

func requestBaseURL(r *http.Request) string {
	scheme := "http"
	if r.TLS != nil || r.Header.Get("X-Forwarded-Proto") == "https" {
		scheme = "https"
	}
	return scheme + "://" + r.Host
}

func (m *multiWebApp) dashboard(w http.ResponseWriter, r *http.Request) {
	items := make([]dashboardInstance, 0, len(m.instances))
	baseURL := requestBaseURL(r)
	for _, app := range m.instances {
		items = append(items, dashboardInstance{
			ID: app.config.ID, Name: app.config.Name,
			Transport: app.config.Transport, Target: app.backend.Target(),
			Path: app.config.BasePath, URL: baseURL + app.config.BasePath + "/5700/",
			UIVariant: app.config.UIVariant,
		})
	}
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	w.Header().Set("Cache-Control", "no-store")
	_ = dashboardTemplate.Execute(w, map[string]any{"Instances": items, "Version": version})
}

func (m *multiWebApp) health(w http.ResponseWriter) {
	instances := make([]map[string]any, 0, len(m.instances))
	for _, app := range m.instances {
		instances = append(instances, map[string]any{
			"id": app.config.ID, "name": app.config.Name, "path": app.config.BasePath,
			"transport": app.config.Transport, "target": app.backend.Target(),
			"ui_variant": app.config.UIVariant,
		})
	}
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.Header().Set("Cache-Control", "no-store")
	_ = json.NewEncoder(w).Encode(map[string]any{
		"ok": true, "service": "mt5700-web-go", "version": version, "instances": instances,
	})
}

func (m *multiWebApp) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	if r.URL.Path == "/nradio/traffic" { serveCellularTraffic(w,r); return }
	w.Header().Set("X-Content-Type-Options", "nosniff")
	w.Header().Set("Referrer-Policy", "same-origin")
	if !m.auth.require(w, r) {
		return
	}
	if r.URL.Path == "/healthz" {
		m.health(w)
		return
	}
	if websocketRequest(r) && r.URL.Path == "/" && len(m.instances) > 0 {
		request := r.Clone(r.Context())
		request.URL.Path = "/"
		m.instances[0].ServeHTTP(w, request)
		return
	}
	if r.URL.Path == "/" {
		m.dashboard(w, r)
		return
	}
	if (r.URL.Path == "/5700" || strings.HasPrefix(r.URL.Path, "/5700/")) && len(m.instances) > 0 {
		http.Redirect(w, r, m.instances[0].config.BasePath+r.URL.Path, http.StatusTemporaryRedirect)
		return
	}
	for basePath, app := range m.byPath {
		if r.URL.Path != basePath && !strings.HasPrefix(r.URL.Path, basePath+"/") {
			continue
		}
		request := r.Clone(r.Context())
		request.URL.Path = strings.TrimPrefix(r.URL.Path, basePath)
		if request.URL.Path == "" {
			request.URL.Path = "/"
		}
		app.ServeHTTP(w, request)
		return
	}
	http.NotFound(w, r)
}

type rotatingLogWriter struct {
	mu      sync.Mutex
	path    string
	maxSize int64
	backups int
	file    *os.File
}

func openRotatingLog(path string, maxSize int64, backups int) (*rotatingLogWriter, error) {
	file, err := os.OpenFile(path, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0640)
	if err != nil {
		return nil, err
	}
	return &rotatingLogWriter{path: path, maxSize: maxSize, backups: backups, file: file}, nil
}

func (w *rotatingLogWriter) rotateLocked() error {
	if w.file != nil {
		_ = w.file.Close()
		w.file = nil
	}
	if w.backups > 0 {
		_ = os.Remove(fmt.Sprintf("%s.%d", w.path, w.backups))
		for index := w.backups - 1; index >= 1; index-- {
			_ = os.Rename(fmt.Sprintf("%s.%d", w.path, index), fmt.Sprintf("%s.%d", w.path, index+1))
		}
		_ = os.Rename(w.path, w.path+".1")
	} else {
		_ = os.Remove(w.path)
	}
	file, err := os.OpenFile(w.path, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0640)
	if err != nil {
		return err
	}
	w.file = file
	return nil
}

func (w *rotatingLogWriter) Write(data []byte) (int, error) {
	w.mu.Lock()
	defer w.mu.Unlock()
	if w.file == nil {
		return 0, os.ErrClosed
	}
	if w.maxSize > 0 {
		if info, err := w.file.Stat(); err == nil && info.Size()+int64(len(data)) > w.maxSize {
			if err := w.rotateLocked(); err != nil {
				return 0, err
			}
		}
	}
	return w.file.Write(data)
}

func (w *rotatingLogWriter) Close() error {
	w.mu.Lock()
	defer w.mu.Unlock()
	if w.file == nil {
		return nil
	}
	err := w.file.Close()
	w.file = nil
	return err
}

func setupLogger(path string, maxSize int64, backups int) func() {
	if path == "" {
		return func() {}
	}
	file, err := openRotatingLog(path, maxSize, backups)
	if err != nil {
		log.Printf("无法打开日志文件 %s：%v", path, err)
		return func() {}
	}
	log.SetOutput(io.MultiWriter(os.Stdout, file))
	return func() { _ = file.Close() }
}

func loadInstanceConfigs(packageName string, defaults serverConfig) ([]serverConfig, bool, error) {
	sections, err := readUCISections(packageName)
	if err != nil {
		return nil, false, err
	}
	configs := make([]serverConfig, 0)
	found := false
	paths := make(map[string]struct{})
	for _, section := range sections {
		if section.Type != "modem" {
			continue
		}
		found = true
		if !optionBool(section.Options, "enabled", true) {
			continue
		}
		instancePath := strings.ToLower(strings.TrimSpace(section.Options["path"]))
		if instancePath == "" {
			instancePath = strings.ToLower(section.Name)
		}
		if !validInstancePath(instancePath) {
			log.Printf("忽略 MT5700 实例 %q：路径 %q 无效", section.Name, instancePath)
			continue
		}
		if _, duplicate := paths[instancePath]; duplicate {
			log.Printf("忽略 MT5700 实例 %q：路径 %q 重复", section.Name, instancePath)
			continue
		}
		paths[instancePath] = struct{}{}
		config := defaults
		config.ID = section.Name
		config.Name = strings.TrimSpace(section.Options["name"])
		if config.Name == "" {
			config.Name = section.Name
		}
		config.BasePath = "/modem/" + instancePath
		config.UIVariant = normalizeUIVariant(section.Options["ui_variant"])
		config.Transport = strings.ToLower(strings.TrimSpace(section.Options["transport"]))
		if config.Transport == "" {
			config.Transport = "serial"
		}
		if config.Transport != "serial" && config.Transport != "network" && config.Transport != "native" {
			log.Printf("忽略 MT5700 实例 %q：AT 类型 %q 无效", section.Name, config.Transport)
			continue
		}
		config.Section = section.Options["modem_section"]
		if config.Section == "" {
			config.Section = "auto"
		}
		config.SerialPort = section.Options["at_port"]
		if config.SerialPort == "" {
			config.SerialPort = "auto"
		}
		config.Baud = optionInt(section.Options, "baudrate", defaults.Baud)
		config.NetworkHost = strings.TrimSpace(section.Options["network_host"])
		if config.NetworkHost == "" {
			config.NetworkHost = defaults.NetworkHost
		}
		config.NetworkPort = optionInt(section.Options, "network_port", defaults.NetworkPort)
		if config.NetworkPort < 1 || config.NetworkPort > 65535 {
			log.Printf("忽略 MT5700 实例 %q：网络 AT 端口 %d 无效", section.Name, config.NetworkPort)
			continue
		}
		config.Timeout = optionDuration(section.Options, "command_timeout", defaults.Timeout)
		config.LongTimeout = optionDuration(section.Options, "long_command_timeout", defaults.LongTimeout)
		if config.LongTimeout < config.Timeout {
			config.LongTimeout = config.Timeout
		}
		config.QueueTimeout = optionDuration(section.Options, "queue_timeout", defaults.QueueTimeout)
		config.WSIdleTimeout = optionDuration(section.Options, "websocket_idle_timeout", defaults.WSIdleTimeout)
		config.MaxClients = optionInt(section.Options, "max_clients", defaults.MaxClients)
		if config.MaxClients < 1 {
			config.MaxClients = defaults.MaxClients
		}
		config.Model = section.Options["model"]
		config.Manufacturer = section.Options["manufacturer"]
		configs = append(configs, config)
	}
	return configs, found, nil
}

func createWebApp(config serverConfig, qmodemHelper string) *webApp {
	discovery := discoverModems(config.Section, config.SerialPort)
	if config.Model == "" {
		config.Model = discovery.SelectedModel
	}
	if config.Manufacturer == "" {
		for _, modem := range discovery.Modems {
			if modem.Section == discovery.SelectedSection {
				config.Manufacturer = modem.Manufacturer
				break
			}
		}
	}
	hub := newHub(config.MaxClients)
	var backend atBackend
	if config.Transport == "network" {
		backend = newNetworkBackend(config.NetworkHost, config.NetworkPort, config.Timeout, config.LongTimeout, hub.broadcastRaw)
	} else if config.Transport == "native" {
		queued := newQModemQueuedBackend(config.Section, config.Section, qmodemHelper, config.Timeout, config.LongTimeout)
		queued.setDiscoveredPort(config.Section)
		backend = queued
	} else {
		queued := newQModemQueuedBackend(config.Section, config.SerialPort, qmodemHelper, config.Timeout, config.LongTimeout)
		queued.setDiscoveredPort(discovery.SelectedPort)
		backend = queued
	}
	return &webApp{config: config, hub: hub, backend: backend}
}

func main() {
	listen := flag.String("listen", "0.0.0.0:9010", "HTTP 与 WebSocket 监听地址")
	transport := flag.String("transport", "serial", "AT 连接类型：serial 或 network")
	section := flag.String("modem-section", "auto", "QModem 配置节")
	serialPort := flag.String("serial", "auto", "AT 串口")
	baud := flag.Int("baud", 115200, "串口波特率")
	networkHost := flag.String("network-host", "192.168.8.1", "网络 AT 主机")
	networkPort := flag.Int("network-port", 20249, "网络 AT 端口")
	timeout := flag.Duration("timeout", 8*time.Second, "AT 命令超时")
	longTimeout := flag.Duration("long-timeout", 240*time.Second, "AT+COPS=? 等长命令超时")
	queueTimeout := flag.Duration("queue-timeout", 60*time.Second, "等待 QModem AT 队列超时")
	wsIdleTimeout := flag.Duration("ws-idle-timeout", 5*time.Minute, "空闲 WebSocket 清理时间")
	qmodemHelper := flag.String("qmodem-helper", "/usr/libexec/mt5700-web/qmodem-at", "QModem AT 队列助手")
	logFile := flag.String("log-file", "/var/log/mt5700-web.log", "日志文件")
	logMaxBytes := flag.Int64("log-max-bytes", 2*1024*1024, "单个日志文件最大字节数")
	logBackups := flag.Int("log-backups", 3, "轮转日志保留份数")
	maxClients := flag.Int("max-clients", 8, "最大 WebSocket 客户端数")
	uciPackage := flag.String("uci-package", "mt5700-web", "多模组实例 UCI 配置包；留空使用单实例参数")
	authEnabled := flag.Bool("auth-enabled", false, "启用控制面板 HTTP Basic 密码认证")
	authUsername := flag.String("auth-username", "admin", "控制面板认证用户名")
	authPasswordHash := flag.String("auth-password-hash", "", "控制面板密码 SHA-256 摘要")
	discover := flag.Bool("discover", false, "输出 QModem 与串口发现结果")
	showVersion := flag.Bool("version", false, "输出版本")
	flag.Parse()

	if *showVersion {
		fmt.Println(version)
		return
	}
	discovery := discoverModems(*section, *serialPort)
	if *discover {
		encoder := json.NewEncoder(os.Stdout)
		encoder.SetIndent("", "  ")
		if err := encoder.Encode(discovery); err != nil {
			log.Fatal(err)
		}
		return
	}
	if *logMaxBytes < 64*1024 {
		*logMaxBytes = 64 * 1024
	}
	if *logBackups < 0 {
		*logBackups = 0
	}
	closeLog := setupLogger(*logFile, *logMaxBytes, *logBackups)
	defer closeLog()
	if *timeout < 500*time.Millisecond {
		*timeout = 500 * time.Millisecond
	}
	if *longTimeout < *timeout {
		*longTimeout = *timeout
	}
	if *queueTimeout < time.Second {
		*queueTimeout = time.Second
	}
	if *wsIdleTimeout < 30*time.Second {
		*wsIdleTimeout = 30 * time.Second
	}
	if *transport != "serial" && *transport != "network" {
		log.Fatalf("不支持的 AT 连接类型：%s", *transport)
	}

	defaults := serverConfig{
		ID: "internal", Name: "内置 MT5700", BasePath: "/modem/internal",
		UIVariant: "modern",
		Listen:    *listen, Transport: *transport, Section: *section,
		SerialPort: *serialPort, Baud: *baud,
		NetworkHost: *networkHost, NetworkPort: *networkPort,
		Timeout: *timeout, LongTimeout: *longTimeout, QueueTimeout: *queueTimeout,
		WSIdleTimeout: *wsIdleTimeout, MaxClients: *maxClients, LogFile: *logFile,
	}
	configs := []serverConfig{defaults}
	if *uciPackage != "" {
		loaded, found, err := loadInstanceConfigs(*uciPackage, defaults)
		if err != nil {
			log.Printf("读取 %s 多模组配置失败，使用兼容单实例：%v", *uciPackage, err)
		} else if found {
			configs = loaded
		}
	}
	passwordHash, err := parsePasswordHash(*authPasswordHash)
	if err != nil {
		log.Fatal(err)
	}
	if *authEnabled && len(passwordHash) != sha256.Size {
		log.Fatal("已启用控制面板密码，但没有有效的 SHA-256 密码摘要")
	}
	username := strings.TrimSpace(*authUsername)
	if username == "" {
		username = "admin"
	}
	multi := &multiWebApp{
		listen:    *listen,
		auth:      authConfig{Enabled: *authEnabled, Username: username, PasswordHash: passwordHash},
		instances: make([]*webApp, 0, len(configs)),
		byPath:    make(map[string]*webApp),
	}
	for _, config := range configs {
		app := createWebApp(config, *qmodemHelper)
		multi.instances = append(multi.instances, app)
		multi.byPath[config.BasePath] = app
		defer app.backend.Close()
	}
	server := &http.Server{
		Addr: *listen, Handler: multi,
		ReadHeaderTimeout: 5 * time.Second,
		IdleTimeout:       90 * time.Second,
		MaxHeaderBytes:    32 * 1024,
	}

	ctx, stop := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
	defer stop()
	startCellularTraffic(ctx)
	go func() {
		<-ctx.Done()
		shutdownCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		_ = server.Shutdown(shutdownCtx)
	}()
	log.Printf("MT5700 Web 控制面板 %s 已监听 %s，启用 %d 个模组实例，密码认证=%t",
		version, *listen, len(multi.instances), multi.auth.Enabled)
	for _, app := range multi.instances {
		log.Printf("MT5700 实例 %s：%s，AT=%s，路径=%s/5700/",
			app.config.ID, app.config.Name, app.backend.Target(), app.config.BasePath)
		if app.config.Model != "" && !mt5700Model(app.config.Model, app.config.Manufacturer) {
			log.Printf("警告：实例 %s 的模组 %q 不是已验证的 MT5700 系列", app.config.ID, app.config.Model)
		}
	}
	if err := server.ListenAndServe(); err != nil && !errors.Is(err, http.ErrServerClosed) {
		log.Fatal(err)
	}
}
