/* Synchronous in <head>: apply the appearance before first paint. */
(() => {
  'use strict';
  const key = 'c2000-appearance';
  const system = matchMedia('(prefers-color-scheme: dark)');
  const compact = matchMedia('(max-width: 700px)');
  const choices = ['system', 'light', 'dark'];
  const skinKey='c2000max-device-art',skinNames={max:'经典银白','788':'马年限定','mid-autumn':'中秋限定','national-day':'国庆限定'};
  const holidayOverrideKey='c2000max-national-day-2026-override';
  let savedSkin='max', holidayOverride=false;
  function readSkinPreference() {
    try {
      const saved=localStorage.getItem(skinKey);
      savedSkin=skinNames[saved]?saved:'max';
      holidayOverride=localStorage.getItem(holidayOverrideKey)==='1';
    } catch {}
  }
  // The campaign is October 1–3, 2026 in Beijing (UTC+8), independent of browser timezone.
  function inNationalHoliday() {
    const day=new Date(Date.now()+8*60*60*1000).toISOString().slice(0,10);
    return day>='2026-10-01' && day<='2026-10-03';
  }
  function preferredSkin() { return inNationalHoliday()&&!holidayOverride?'national-day':savedSkin; }
  readSkinPreference();
  let skin=preferredSkin();
  function refreshScheduledSkin() {
    const next=preferredSkin();
    if(next===skin)return;
    skin=next;apply();document.dispatchEvent(new CustomEvent('c2000-skin-change',{detail:skin}));
  }
  let preference = 'system';
  try { const saved = localStorage.getItem(key); if (choices.includes(saved)) preference = saved; } catch {}
  function apply() {
    const resolved = preference === 'system' ? (system.matches ? 'dark' : 'light') : preference;
    document.documentElement.dataset.theme = resolved;
    document.documentElement.dataset.skin = skin;
    document.documentElement.dataset.appearance = preference;
    document.querySelectorAll('[data-theme-choice]').forEach(button => button.setAttribute('aria-pressed', String(button.dataset.themeChoice === preference)));
    document.querySelectorAll('[data-theme-toggle]').forEach(toggle => {
      const selected = toggle.parentElement.querySelector('[data-theme-choice="' + preference + '"]');
      if(!selected)return;
      const name = selected.getAttribute('aria-label');
      toggle.replaceChildren(selected.querySelector('svg').cloneNode(true));
      toggle.setAttribute('aria-label', skinNames[skin]+' · 外观模式：' + name);
      toggle.title = skinNames[skin]+' · 外观模式：' + name;
    });
    document.querySelectorAll('.theme-festival').forEach(panel => {
      panel.hidden = skin === 'max';
      panel.querySelector('strong').textContent = skinNames[skin];
      panel.querySelector('small').textContent = skin === 'mid-autumn' ? '桂香月满，好事相伴' : skin === '788' ? '骏马迎春，喜乐常在' : '锦绣山河，共赴美好';
      panel.querySelector('img').src = (window.L?.env?.media || '/luci-static/c2000max-ui') + '/assets/mascot-' + (skin === 'mid-autumn' ? 'rabbit' : skin === '788' ? 'horse-flat' : 'lion') + '.png';
    });
    document.querySelector('meta[name="theme-color"]')?.setAttribute('content', resolved === 'dark' ? '#111111' : '#eef3f8');
  }
  function set(value) {
    if (!choices.includes(value)) return;
    preference = value;
    try { localStorage.setItem(key, value); } catch {}
    apply();
  }
  function setSkin(value) {
    if(!skinNames[value])return;
    savedSkin=skin=value;
    if(inNationalHoliday())holidayOverride=true;
    try {
      if(holidayOverride)localStorage.setItem(holidayOverrideKey,'1');
      localStorage.setItem(skinKey,value);
    } catch {}
    apply();document.dispatchEvent(new CustomEvent('c2000-skin-change',{detail:skin}));
  }
  function closePickers(restoreFocus = false) {
    document.querySelectorAll('[data-theme-toggle][aria-expanded="true"]').forEach(toggle => {
      toggle.setAttribute('aria-expanded', 'false');
      if (restoreFocus && compact.matches) toggle.focus();
    });
  }
  apply();
  system.addEventListener('change', apply);
  window.addEventListener('storage', event => {
    if(event.key===skinKey||event.key===holidayOverrideKey||event.key===null){readSkinPreference();refreshScheduledSkin();}
    if (event.key === key || event.key === null) { preference = choices.includes(event.newValue) ? event.newValue : 'system'; apply(); }
  });
  document.addEventListener('DOMContentLoaded', apply);
  // A long-lived dashboard also enters/leaves the campaign; resumed tabs refresh immediately.
  setInterval(refreshScheduledSkin,60000);
  document.addEventListener('visibilitychange',()=>{if(!document.hidden)refreshScheduledSkin();});
  window.addEventListener('pageshow',refreshScheduledSkin);
  document.addEventListener('click', event => {
    const toggle = event.target.closest('[data-theme-toggle]');
    if (toggle) {
      const opening = toggle.getAttribute('aria-expanded') !== 'true';
      closePickers();
      toggle.setAttribute('aria-expanded', String(opening));
      if (opening) toggle.parentElement.querySelector('[aria-pressed="true"]').focus();
      return;
    }
    const button = event.target.closest('[data-theme-choice]');
    if (button) { set(button.dataset.themeChoice); closePickers(true); }
    else if (!event.target.closest('.theme-picker')) closePickers();
  });
  document.addEventListener('keydown', event => {
    if (event.key === 'Escape') closePickers(true);
  });
  document.addEventListener('focusin', event => {
    if (!event.target.closest('.theme-picker')) closePickers();
  });
  compact.addEventListener('change', () => {
    const toggleFocused = document.activeElement?.matches('[data-theme-toggle]');
    closePickers();
    if (!compact.matches && toggleFocused) document.querySelector('[data-theme-choice][aria-pressed="true"]')?.focus();
  });
  window.C2000Theme = Object.freeze({ set, get: () => preference, setSkin, getSkin:()=>skin });
})();
