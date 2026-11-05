/* NBCPE integration only; the WebUI application is C5800's upstream 3.0.2 build. */
(function () {
  localStorage.setItem('atHost', location.hostname);
  localStorage.setItem('atPort', '8766');
  if (window.self === window.top) return;
  document.documentElement.classList.add('nbcpe-embedded');
  function start() {
    var pending = false, lastHeight = 0;
    function measure() {
      pending = false;
      var root = document.getElementById('root');
      if (!root) return;
      var height = Math.ceil(Math.max(root.getBoundingClientRect().height, root.scrollHeight) + 8);
      if (height === lastHeight) return;
      lastHeight = height;
      window.parent.postMessage({type:'myos-nbcpe-webui-height',height:height}, location.origin);
    }
    function schedule() {
      if (!pending) { pending = true; requestAnimationFrame(measure); }
    }
    new MutationObserver(schedule).observe(document.body, {childList:true,subtree:true});
    new ResizeObserver(schedule).observe(document.getElementById('root'));
    window.addEventListener('resize', schedule);
    schedule();
  }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', start);
  else start();
})();
