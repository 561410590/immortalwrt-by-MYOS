'use strict';
'require view';
'require uci';

return view.extend({
    handleSaveApply: null,
    handleSave: null,
    handleReset: null,

    load: function() {
        return uci.load('embed_simcard');
    },

    render: function() {
        var defaultUrl = window.location.origin + '/mt5700/5700/';
        var currentUrl = uci.get('embed_simcard', 'webui', 'url') || defaultUrl;
        var savedUrl;
        try { savedUrl = new URL(currentUrl, window.location.origin); } catch (e) {}
        if (savedUrl && savedUrl.origin === window.location.origin &&
            /^\/5700(?:\/|$)/.test(savedUrl.pathname)) {
            currentUrl = window.location.origin + '/mt5700' + savedUrl.pathname +
                (savedUrl.pathname === '/5700' ? '/' : '') + savedUrl.search + savedUrl.hash;
        }
        var tabBase = 'display:inline-block;width:100px;height:32px;line-height:32px;' +
            'text-align:center;cursor:pointer;border:1px solid #ccc;border-bottom:none;' +
            'border-radius:4px 4px 0 0;margin-right:4px;font-size:13px;background:#eee;' +
            'color:#555;user-select:none;box-sizing:border-box;';
        var tabActive = tabBase + 'background:#fff;color:#000;font-weight:bold;';
        var frameEl = E('iframe', {
            src: currentUrl,
            title: _('模组 WebUI'),
            style: 'width:100%;height:100%;min-width:0;min-height:0;border:0;display:block;'
        });
        var pageWebui = E('div', {
            style: 'display:block;flex:1;min-width:0;min-height:0;height:100%;overflow:hidden;'
        }, [frameEl]);
        var urlInput = E('input', {
            type: 'text', value: currentUrl,
            style: 'width:100%;padding:6px 8px;font-size:13px;border:1px solid #ccc;' +
                'border-radius:3px;box-sizing:border-box;'
        });
        var applyBtn = E('button', {
            class: 'btn cbi-button cbi-button-apply', style: 'margin-top:10px;',
            click: function() {
                var newUrl = urlInput.value.trim();
                if (!newUrl) return;
                uci.set('embed_simcard', 'webui', 'url', newUrl);
                uci.save().then(function() {
                    frameEl.src = newUrl;
                    switchTab('webui');
                }).catch(function(err) {
                    window.alert(_('保存失败：') + (err.message || err));
                });
            }
        }, [_('保存并跳转')]);
        var resetBtn = E('button', {
            class: 'btn cbi-button', style: 'margin-top:10px;margin-left:8px;',
            click: function() { urlInput.value = defaultUrl; }
        }, [_('重置为默认')]);
        var pageSettings = E('div', {
            style: 'display:none;padding:20px;box-sizing:border-box;overflow:auto;'
        }, [
            E('h4', { style: 'margin:0 0 10px 0;' }, [_('配置webui地址')]), urlInput,
            E('div', { style: 'font-size:12px;color:#888;margin-top:5px;' }, [
                _('当前访问地址：') + window.location.origin
            ]),
            E('div', { style: 'margin-top:4px;' }, [applyBtn, resetBtn])
        ]);
        var tabWebui = E('span', { style: tabActive }, [_('WebUI')]);
        var tabSettings = E('span', { style: tabBase }, [_('⚙ 设置')]);
        function switchTab(name) {
            var isWebui = name === 'webui';
            tabWebui.style.cssText = isWebui ? tabActive : tabBase;
            tabSettings.style.cssText = isWebui ? tabBase : tabActive;
            pageWebui.style.display = isWebui ? 'block' : 'none';
            pageSettings.style.display = isWebui ? 'none' : 'block';
        }
        tabWebui.addEventListener('click', function() { switchTab('webui'); });
        tabSettings.addEventListener('click', function() { switchTab('settings'); });
        var tabBar = E('div', {
            style: 'display:flex;flex:none;align-items:flex-end;padding:6px 0 0 0;' +
                'border-bottom:1px solid #ccc;'
        }, [tabWebui, tabSettings]);
        var content = E('div', {
            style: 'flex:1;min-width:0;min-height:0;overflow:hidden;display:flex;flex-direction:column;'
        }, [pageWebui, pageSettings]);
        var wrapper = E('div', {
            class: 'myos-embedded-webui',
            style: 'display:flex;flex-direction:column;min-width:0;min-height:0;overflow:hidden;'
        }, [tabBar, content]);
        function resizeFrame() {
            if (!wrapper.isConnected) {
                window.removeEventListener('resize', resizeFrame);
                return;
            }
            var bottom = window.innerHeight - 16;
            var mobileNav = document.querySelector('.mobile-nav');
            if (mobileNav) {
                var navRect = mobileNav.getBoundingClientRect();
                if (navRect.height && navRect.top > 0 && navRect.top < bottom)
                    bottom = navRect.top - 12;
            }
            var main = wrapper.closest('main');
            var footer = main && main.querySelector('.main-footer');
            if (footer && footer.getBoundingClientRect().height) {
                var footerStyle = window.getComputedStyle(footer);
                bottom -= footer.getBoundingClientRect().height +
                    (parseFloat(footerStyle.marginTop) || 0) +
                    (parseFloat(footerStyle.marginBottom) || 0);
            }
            if (main) bottom -= parseFloat(window.getComputedStyle(main).paddingBottom) || 0;
            var bodyStyle = window.getComputedStyle(document.body);
            bottom -= (parseFloat(bodyStyle.marginBottom) || 0) +
                (parseFloat(bodyStyle.paddingBottom) || 0);
            wrapper.style.height = Math.max(160, bottom - wrapper.getBoundingClientRect().top - window.scrollY) + 'px';
        }
        window.addEventListener('resize', resizeFrame);
        frameEl.addEventListener('load', resizeFrame);
        window.requestAnimationFrame(resizeFrame);
        return wrapper;
    }
});
