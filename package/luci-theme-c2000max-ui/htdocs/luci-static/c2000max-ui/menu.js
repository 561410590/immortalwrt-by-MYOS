(() => {
  'use strict';
  const sidebar = document.querySelector('#sidebar');
  const nav = document.querySelector('#menu-tree');
  const search = document.querySelector('#menu-search');
  const backdrop = document.querySelector('.sidebar-backdrop');
  const narrow = matchMedia('(max-width: 1100px)');
  const icons = {
    status: 'grid', system: 'settings', modem: 'radio', services: 'layers',
    network: 'globe', statistics: 'chart', vpn: 'shield', nas: 'storage',
    store: 'package', istore: 'package', software: 'package', opkg: 'package',
    package: 'package', packagemanager: 'package', logout: 'logout',
    embedded: 'sim', simcard: 'sim', cpesel: 'sim', qmodem: 'radio', qmodemnext: 'radio',
    atwebserver: 'terminal', ttyd: 'terminal', terminal: 'terminal',
    fancontrol: 'fan', firewall: 'shield', openclash: 'shield', passwall: 'shield',
    passwall2: 'shield', wireguard: 'shield', openvpn: 'shield', zerotier: 'shield',
    tailscale: 'shield', ddns: 'cloud', nros: 'cloud', myosnros: 'cloud',
    myosupdate: 'refresh', update: 'refresh', flash: 'upload', backup: 'download',
    wireless: 'wifi', interfaces: 'globe', dhcp: 'globe', routes: 'globe',
    clients: 'people', devices: 'people', processes: 'activity', realtime: 'chart',
    wrtbwmon: 'chart', traffic: 'chart', logs: 'terminal', leds: 'signal'
  };
  const iconTones = {
    home: 'network', grid: 'network', globe: 'network', settings: 'system',
    radio: 'device', layers: 'device', device: 'device', chart: 'signal',
    package: 'device', plug: 'device', shield: 'network', storage: 'device',
    terminal: 'system', fan: 'signal', sim: 'device', cloud: 'network',
    refresh: 'signal', wifi: 'network', people: 'network', activity: 'signal',
    upload: 'system', download: 'network', signal: 'signal'
  };
  function menuIcon(node, grouped = false) {
    const key = String(node.name || '').toLowerCase().replace(/[-_]/g, '');
    if (icons[key]) return icons[key];
    const label = key + ' ' + String(node.title || '').toLowerCase();
    if (/istore|软件包|软件商店/.test(label)) return 'package';
    if (/sim|切卡|内置卡/.test(label)) return 'sim';
    if (/fan|风扇/.test(label)) return 'fan';
    if (/atweb|at web|ttyd|terminal|终端/.test(label)) return 'terminal';
    if (/qmodem|modem|调制解调|模组/.test(label)) return 'radio';
    if (/openclash|passwall|wireguard|openvpn|zerotier|tailscale|vpn|firewall|防火墙/.test(label)) return 'shield';
    if (/ddns|nros|云控|远控|远程控制|动态域名/.test(label)) return 'cloud';
    if (/固件更新|在线升级|myosupdate/.test(label)) return 'refresh';
    if (/无线|wireless|wifi/.test(label)) return 'wifi';
    if (/流量|带宽|traffic|bandwidth/.test(label)) return 'chart';
    if (/日志|log/.test(label)) return 'terminal';
    return grouped ? 'layers' : 'plug';
  }

  let opener, currentTree, currentOptions, openBeforeSearch = new Set();
  const svg = name => { const node = document.createElementNS('http://www.w3.org/2000/svg','svg'); node.setAttribute('class','icon' + (iconTones[name] ? ' icon-tone-' + iconTones[name] : '')); node.setAttribute('aria-hidden','true'); const use = document.createElementNS(node.namespaceURI,'use'); const id = document.getElementById('menu-' + name) ? 'menu-' + name : name; use.setAttribute('href','#' + (document.getElementById(id) ? id : 'layers')); node.append(use); return node; };
  const text = (tag, value, className) => { const node = document.createElement(tag); node.textContent = value; if (className) node.className = className; return node; };
  const defaultChildren = node => Object.entries(node.children || {}).map(([name, child]) => ({ ...child, name })).filter(child => child.title && !child.hidden && child.satisfied !== false).sort((a,b) => (a.order || 0) - (b.order || 0));
  const inertTargets = () => document.querySelectorAll('.main, .brand, .header-menu, .mobile-nav');

  function setDrawer(open, returnFocus = true) {
    if (open && !narrow.matches) return;
    if (open) opener = document.activeElement;
    document.body.classList.toggle('menu-open', open);
    document.querySelectorAll('[data-menu-toggle]').forEach(button => button.setAttribute('aria-expanded', String(open)));
    backdrop.hidden = !open;
    sidebar.inert = narrow.matches && !open;
    if (open) { sidebar.setAttribute('role','dialog'); sidebar.setAttribute('aria-modal','true'); }
    else { sidebar.removeAttribute('role'); sidebar.removeAttribute('aria-modal'); }
    inertTargets().forEach(element => { element.inert = open; });
    if (open) document.querySelector('#close-menu').focus();
    else if (returnFocus && narrow.matches && opener?.isConnected && opener.getClientRects().length) opener.focus();
  }

  function filterMenu() {
    const query = search.value.trim().toLocaleLowerCase();
    if (!nav.dataset.searching && query) openBeforeSearch = new Set([...nav.querySelectorAll('details[open]')].map(item => item.dataset.path));
    function visit(container, inheritedMatch = false) {
      let found = false;
      for (const item of container.children) {
        if (!item.dataset.menuItem) continue;
        const matches = inheritedMatch || !query || item.dataset.title.toLocaleLowerCase().includes(query);
        const childList = item.querySelector(':scope > .menu-children');
        const childMatch = childList ? visit(childList, matches && !!query) : false;
        item.hidden = !(matches || childMatch);
        if (item.tagName === 'DETAILS') {
          if (query) item.open = !item.hidden;
          else if (nav.dataset.searching) item.open = openBeforeSearch.has(item.dataset.path);
        }
        found = found || !item.hidden;
      }
      return found;
    }
    document.querySelector('.menu-empty').hidden = visit(nav);
    if (query) nav.dataset.searching = 'true'; else delete nav.dataset.searching;
  }

  function mount(tree, options = {}) {
    currentTree = tree; currentOptions = options;
    const previous = new Set([...nav.querySelectorAll('details[open]')].map(item => item.dataset.path));
    const firstRender = !nav.children.length;
    const childrenOf = options.getChildren || defaultChildren;
    const translate = options.translate || (value => value);
    const url = options.url || (path => '/cgi-bin/luci/' + path.map(encodeURIComponent).join('/'));
    const activePath = options.activePath || location.pathname;
    const root = tree.children?.admin || tree;
    const fragment = document.createDocumentFragment();
    const home = text('a', '', 'nav-home nav-item');
    home.href = options.homeUrl || '#home'; home.dataset.previewPanel = 'home';
    const homePath = new URL(home.href, location.href).pathname.replace(/\/$/, '');
    const menuChildren = (node, path) => childrenOf(node).filter(child =>
      options.demo || (!child.hidden &&
        !['admin/status/c5800','admin/status/c2000max','admin/status/c2000ultra','admin/modeminfo'].includes([...path, child.name].join('/')) &&
        new URL(url([...path, child.name]), location.href).pathname.replace(/\/$/, '') !== homePath));
    home.append(svg('home'), text('span','首页'));
    if (options.demo || activePath === new URL(home.href, location.href).pathname) { home.classList.add('active'); home.setAttribute('aria-current','page'); }
    fragment.append(home);
    function makeLink(node, path, title) {
      const anchor = text('a', '', 'menu-link'); anchor.href = url(path);
      anchor.append(svg(menuIcon(node)), text('span', title));
      anchor.dataset.menuItem = 'true'; anchor.dataset.title = title;
      anchor.dataset.previewPanel = node.previewPanel || '';
      anchor.dataset.path = path.join('/');
      const pathname = new URL(anchor.href, location.href).pathname.replace(/\/$/,'');
      if (!options.demo && activePath.replace(/\/$/,'') === pathname) { anchor.classList.add('active'); anchor.setAttribute('aria-current','page'); }
      return anchor;
    }
    function render(node, path, depth) {
      const title = translate(node.title);
      const children = menuChildren(node, path);
      if (!children.length) { const link = makeLink(node, path, title); if (depth === 0) link.classList.add('menu-root-link'); return link; }
      const group = document.createElement('details'); group.className = 'menu-group';
      group.dataset.menuItem = 'true'; group.dataset.title = title; group.dataset.path = path.join('/');
      const summary = document.createElement('summary'); summary.className = 'menu-category';
      summary.append(svg(menuIcon(node, true)));
      summary.append(text('span',title), svg('chevron'));
      const list = document.createElement('div'); list.className = 'menu-children';
      // A navigable parent with children retains its own entry.
      if (node.action && !['firstchild','alias'].includes(node.action.type)) list.append(makeLink(node, path, title + '概览'));
      children.forEach(child => list.append(render(child, [...path, child.name], depth + 1)));
      group.append(summary,list);
      group.open = previous.has(group.dataset.path) || !!list.querySelector('[aria-current="page"]') || (options.demo && firstRender && node.name === 'status');
      return group;
    }
    menuChildren(root, ['admin']).forEach(child => fragment.append(render(child, ['admin',child.name], 0)));
    nav.replaceChildren(fragment);
    if (!options.demo) {
      const currentPath = activePath.replace(/\/$/,'');
      const matches = [...nav.querySelectorAll('a')].map(link => ({ link, path: new URL(link.href,location.href).pathname.replace(/\/$/,'') })).filter(item => currentPath === item.path || currentPath.startsWith(item.path + '/')).sort((a,b) => b.path.length - a.path.length);
      nav.querySelectorAll('a').forEach(link => { link.removeAttribute('aria-current'); link.classList.remove('active'); });
      const active = matches[0]?.link;
      if (active) {
        active.setAttribute('aria-current','page'); active.classList.add('active');
        for (let parent = active.parentElement; parent && parent !== nav; parent = parent.parentElement) if (parent.tagName === 'DETAILS') parent.open = true;
      }
    }
    nav.dataset.demo = String(!!options.demo);
    document.querySelector('#menu-source').textContent = 'LuCI · ' + (document.body.dataset.deviceModel || 'MYOS');
    filterMenu();
  }

  nav.addEventListener('click', event => {
    const link = event.target.closest('a'); if (!link) return;
    if (nav.dataset.demo === 'true') {
      event.preventDefault();
      setDrawer(false);
      document.dispatchEvent(new CustomEvent('c2000:menu-select', { detail: { panel: link.dataset.previewPanel, title: link.textContent, path: link.dataset.path } }));
    } else setDrawer(false, false);
  });
  search.addEventListener('input', filterMenu);
  search.addEventListener('keydown', event => { if (event.key === 'Escape' && search.value) { event.stopPropagation(); search.value = ''; filterMenu(); } });
  document.querySelectorAll('[data-menu-toggle]').forEach(button => button.addEventListener('click', () => setDrawer(!document.body.classList.contains('menu-open'))));
  document.querySelector('#close-menu').addEventListener('click', () => setDrawer(false));
  backdrop.addEventListener('click', () => setDrawer(false));
  document.addEventListener('keydown', event => {
    if (!document.body.classList.contains('menu-open')) return;
    if (event.key === 'Escape') { event.preventDefault(); setDrawer(false); }
    if (event.key !== 'Tab') return;
    const focusable = [...sidebar.querySelectorAll('button, input, a, summary')].filter(item => {
      if (!item.getClientRects().length || item.closest('[hidden]') || item.disabled || getComputedStyle(item).visibility === 'hidden') return false;
      // Closed details can still report descendant rectangles. Only its summary is focusable.
      for (let parent = item.parentElement; parent && parent !== sidebar; parent = parent.parentElement) {
        if (parent.tagName === 'DETAILS' && !parent.open && parent.firstElementChild !== item) return false;
      }
      return true;
    });
    const first = focusable[0], last = focusable.at(-1);
    if (event.shiftKey && document.activeElement === first) { event.preventDefault(); last.focus(); }
    else if (!event.shiftKey && document.activeElement === last) { event.preventDefault(); first.focus(); }
  });
  narrow.addEventListener('change', () => setDrawer(false, false));
  setDrawer(false, false);
  window.C2000Menu = Object.freeze({ setTree: mount, open: () => setDrawer(true), close: () => setDrawer(false), refresh: () => currentTree && mount(currentTree,currentOptions) });

  async function initialize() {
    if (window.L && typeof L.require === 'function') {
      try {
        const ui = await L.require('ui');
        ui.menu.flushCache();
        ui.menu.menu = null;
        const tree = await ui.menu.load();
        mount(tree, { getChildren: node => ui.menu.getChildren(node), url: path => L.url(...path), translate: value => typeof window._ === 'function' ? window._(value) : value, homeUrl: document.body.dataset.homeUrl });
        return tree;
      } catch {
        nav.replaceChildren(text('p','菜单暂时无法加载，请刷新重试。','menu-error'));
        document.querySelector('#menu-source').textContent = 'LuCI 菜单加载失败';
      }
    } else if (window.C2000_DEMO_MENU) mount(window.C2000_DEMO_MENU, { demo: true });
  }
  window.C2000MenuReady = initialize();
})();
