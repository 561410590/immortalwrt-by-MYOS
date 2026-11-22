import {bitsText, packetsText, timeText, seriesFromCounters, latestPoint} from './iface-samples.js?v=3';

function make(tag, className, text) {
    const item = document.createElement(tag);
    if (className) item.className = className;
    if (text != null) item.textContent = text;
    return item;
}

function selectField(title, options) {
    const select = make('select', 'cbi-input-select');
    select.setAttribute('aria-label', title);
    for (const [value, label] of options)
        select.add(new Option(label, value));
    return select;
}

export function chartSection() {
    const root = make('section', 'ifmon-trend');
    const heading = make('div', 'ifmon-section-head');
    const title = make('h3', '', '接口流量趋势');
    const controls = make('div', 'ifmon-inline');
    const mode = selectField('图表指标', [['bits', '收发速率'], ['packets', '收发包速率']]);
    const period = selectField('显示时段', [['60', '最近 1 分钟'], ['180', '最近 3 分钟']]);
    controls.append(mode, period);
    heading.append(title, controls);

    const output = make('div', 'ifmon-chart-readings');
    const stamp = make('span', 'ifmon-chart-stamp');
    const rxText = make('span', 'ifmon-rx');
    const txText = make('span', 'ifmon-tx');
    output.append(stamp, rxText, txText);
    const holder = make('div', 'ifmon-chart-holder');
    holder.tabIndex = 0;
    holder.setAttribute('role', 'group');
    holder.setAttribute('aria-label', '接口采样曲线。用左右方向键查看读数，Esc 回到最新。');
    const status = make('p', 'ifmon-note');
    root.append(heading, output, holder, status);

    let plot, library, observer, counters = [], data, selectedTime;
    let generation = 0, visible = false, failure = '', sampleClock = null, maxGap = 5;
    const shades = () => {
        const theme = getComputedStyle(root);
        const color = (key, fallback) => theme.getPropertyValue(key).trim() || fallback;
        return {
            receive: color('--m-chart-rx', '#14785f'),
            transmit: color('--m-chart-tx', '#1967c9'),
            text: color('--m-muted', '#5c6878'),
            line: color('--m-line', '#dde1e5')
        };
    };

    function detail(index) {
        const selected = Number.isInteger(index) && data && index >= 0 && index < data.times.length;
        const packetMode = mode.value === 'packets';
        const point = selected ? {
            time: data.times[index],
            rx: (packetMode ? data.rxPackets : data.rxBits)[index],
            tx: (packetMode ? data.txPackets : data.txBits)[index]
        } : latestPoint(data, mode.value);
        const format = packetMode ? packetsText : bitsText;
        const stale = !selected && point.time != null && sampleClock != null && sampleClock - point.time > 8;
        stamp.textContent = point.time == null ? '等待采样' :
            (selected ? '所选 ' : stale ? '上次采样 ' : '最新 ') + timeText(point.time);
        rxText.textContent = '接收 ' + format(point.rx);
        txText.textContent = '发送 ' + format(point.tx);
    }

    function chartOptions() {
        const palette = shades();
        const format = mode.value === 'packets' ? packetsText : bitsText;
        const height = holder.clientHeight || 180;
        return {
            width: Math.max(1, holder.clientWidth),
            height,
            padding: [10, 12, 2, 0],
            legend: {show: false},
            select: {show: false},
            cursor: {
                y: false,
                drag: {x: false, y: false, setScale: false},
                focus: {prox: -1},
                lock: false
            },
            scales: {
                x: {time: false},
                y: {range: (_chart, _low, high) => [0, !Number.isFinite(high) || high <= 0 ? 1 : high * 1.07]}
            },
            axes: [
                {
                    stroke: palette.text,
                    size: 29,
                    space: 82,
                    font: '11px sans-serif',
                    ticks: {show: false},
                    grid: {show: false},
                    values: (_plot, ticks) => ticks.map(timeText)
                },
                {
                    stroke: palette.text,
                    size: 84,
                    space: 34,
                    font: '11px sans-serif',
                    grid: {stroke: palette.line, width: 1, dash: [2, 5]},
                    ticks: {show: false},
                    values: (_plot, ticks) => ticks.map(value => format(value))
                }
            ],
            series: [
                {},
                {label: '接收', stroke: palette.receive, width: 2, spanGaps: false, points: {size: 4}},
                {label: '发送', stroke: palette.transmit, width: 2, dash: [5, 3], spanGaps: false, points: {size: 4}}
            ],
            hooks: {
                setCursor: [view => {
                    if (!data || !visible) return;
                    if (selectedTime == null) detail(view.cursor.idx);
                }]
            }
        };
    }

    function discardPlot() {
        if (plot) plot.destroy();
        plot = null;
    }

    function showPoint(index) {
        if (!plot || !data.times.length || index < 0) return;
        selectedTime = data.times[index];
        plot.setCursor({left: plot.valToPos(selectedTime, 'x'), top: 0});
        detail(index);
    }

    function render() {
        if (!visible) return;
        data = seriesFromCounters(counters, Number(period.value), maxGap);
        const hasPacketCounters = counters.some(row => Array.isArray(row) && Number.isFinite(row[2]) && Number.isFinite(row[4]));
        mode.options[1].disabled = !hasPacketCounters;
        if (!hasPacketCounters && mode.value === 'packets') {
            mode.value = 'bits';
            discardPlot();
        }
        const packetMode = mode.value === 'packets';
        const ys = packetMode ? [data.rxPackets, data.txPackets] : [data.rxBits, data.txBits];
        const hasData = ys.some(values => values.some(Number.isFinite));
        holder.hidden = !hasData || !library;
        status.textContent = failure || (!hasData ? '等待连续的接口计数采样。' :
            !library ? '正在加载曲线…' : '');
        status.hidden = !status.textContent;
        if (!hasData || !library) {
            discardPlot();
            selectedTime = null;
            detail();
            return;
        }
        const points = [data.times, ...ys];
        if (!plot) plot = new library(chartOptions(), points, holder);
        else plot.setData(points);
        const selected = selectedTime == null ? -1 : data.times.indexOf(selectedTime);
        if (selected >= 0) showPoint(selected);
        else {
            selectedTime = null;
            detail(plot.cursor.idx);
        }
    }

    holder.addEventListener('pointermove', () => { selectedTime = null; });
    holder.addEventListener('pointerleave', () => {
        if (selectedTime == null && data) detail();
    });
    holder.addEventListener('keydown', event => {
        if (!plot || !data.times.length) return;
        if (event.key === 'Escape' && selectedTime != null) {
            event.preventDefault();
            event.stopPropagation();
            selectedTime = null;
            plot.setCursor({left: -10, top: -10});
            detail();
            return;
        }
        if (!['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) return;
        event.preventDefault();
        const last = data.times.length - 1;
        const current = selectedTime == null ? last : data.times.indexOf(selectedTime);
        if (event.key === 'Home') showPoint(0);
        else if (event.key === 'End') showPoint(last);
        else showPoint(Math.min(last, Math.max(0, current + (event.key === 'ArrowLeft' ? -1 : 1))));
    });
    mode.addEventListener('change', () => {discardPlot(); render();});
    period.addEventListener('change', render);

    return {
        root,
        start() {
            const request = ++generation;
            visible = true;
            counters = [];
            selectedTime = null;
            failure = '';
            if (window.ResizeObserver) {
                observer = new ResizeObserver(() => {
                    if (!plot || !holder.clientWidth || !holder.clientHeight) return;
                    if (plot.width !== holder.clientWidth || plot.height !== holder.clientHeight)
                        plot.setSize({width: holder.clientWidth, height: holder.clientHeight});
                });
                observer.observe(holder);
            }
            render();
            if (!library)
                import('./uplot-1.6.32.js').then(module => {
                    library = module.default;
                    if (request === generation) render();
                }).catch(() => {
                    if (request !== generation) return;
                    failure = '曲线组件未能加载。';
                    render();
                });
        },
        update(samples, now, message, intervalLimit = 5) {
            maxGap = intervalLimit;
            counters = Array.isArray(samples) ? samples : [];
            sampleClock = now;
            failure = message || '';
            render();
        },
        stop() {
            ++generation;
            visible = false;
            observer?.disconnect();
            observer = null;
            counters = [];
            selectedTime = null;
            discardPlot();
        }
    };
}
