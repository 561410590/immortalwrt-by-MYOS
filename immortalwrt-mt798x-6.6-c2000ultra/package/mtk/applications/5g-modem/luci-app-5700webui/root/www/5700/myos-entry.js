(function () {
    'use strict';
    // Keep the factory WebUI connected to the router serving this page.
    var entryHost = window.location.hostname;
    window.localStorage.setItem('atHost', entryHost);
    var originalFetch = window.fetch;
    window.fetch = function (input, init) {
        var requestUrl = new URL(typeof input === 'string' ? input : input.url, window.location.href);
        var result = originalFetch.call(this, input, init);
        if (requestUrl.origin !== window.location.origin || requestUrl.pathname !== '/5700/config.json')
            return result;
        return result.then(function (response) {
            if (!response.ok) return response;
            return response.clone().json().then(function (config) {
                if (config.status !== 'true' || !config.at) return response;
                config.at.host = entryHost;
                var headers = new Headers(response.headers);
                headers.delete('Content-Length');
                return new Response(JSON.stringify(config), {
                    status: response.status, statusText: response.statusText, headers: headers
                });
            }).catch(function () { return response; });
        });
    };
})();

