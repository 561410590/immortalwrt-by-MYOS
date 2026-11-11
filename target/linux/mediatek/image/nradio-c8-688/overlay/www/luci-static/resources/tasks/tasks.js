
(function(){
    const taskd={};
    const $gettext = function(str) {
        return taskd.i18n[str] || str;
    };
    const retryPromise = function(fn) {
        return new Promise((resolve, reject) => {
            const retry = function() {
                fn(resolve, reject, retry);
            };
            retry();
        });
    };
    const retry403XHR = function(url, method, responseType) {
        return retryPromise((resolve, reject, retry) => {
            var oReq = new XMLHttpRequest();
            oReq.onerror = reject;
            oReq.open(method || 'GET', url, true);
            if (responseType) {
                oReq.responseType = responseType;
            }
            oReq.onload = function (oEvent) {
                if (oReq.status == 403) {
                    alert($gettext("Lost login status"));
                    location.href = location.href.replace(/#.*$/,'');
                } else if (oReq.status >= 400) {
                    reject(oEvent);
                } else {
                    resolve(oReq);
                }
            };
            if (method=='POST') {
                oReq.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
            }
            oReq.send(method=='POST'?("token="+taskd.csrfToken):null);
        });
    };
    const request = function(url, method) {
        return retry403XHR(url, method).then(oReq => oReq.responseText);
    };
    const getBin = function(url) {
        return retry403XHR(url, null, "arraybuffer").then(oReq => {return {status: oReq.status, buffer: new Uint8Array(oReq.response)}});
    };
    const getTaskDetail = function(task_id) {
        return request("/cgi-bin/luci/admin/system/tasks/status?task_id="+task_id).then(data=>JSON.parse(data));
    };
    const create_dialog = function(cfg) {
        const container = document.createElement('div');
        container.id = "tasks_detail_container";
        container.innerHTML = taskd.dialog_template;

        document.body.appendChild(container);
        const title_view = container.querySelector(".dialog-title-bar .dialog-title");
        title_view.innerText = cfg.title;

        const term = new Terminal({convertEol: cfg.convertEol||false});
        if (cfg.nohide) {
            container.querySelector(".dialog-icon-min").hidden = true;
        } else {
            container.querySelector(".dialog-icon-min").onclick = function(){
                container.hidden=true;
                term.dispose();
                document.body.removeChild(container);
                cfg.onhide && cfg.onhide();
                return false;
            };
        }
        const tasks_result_mask = container.querySelector("#tasks_result_mask");
        if (taskd.show_mask_on_stopped) {
            tasks_result_mask.onclick = function(){
                tasks_result_mask.hidden=true;
            };
        } else {
            tasks_result_mask.hidden=true;
        }
        term.open(container.querySelector("#tasks_xterm_log"));

        let logParts = [];
        let decoder = new TextDecoder();
        const logText = () => logParts.join('').replace(/\x1b\[[0-?]*[ -/]*[@-~]/g, '');
        const actions = document.createElement('div');
        actions.className = 'tasks-log-actions';
        actions.style.cssText = 'display:flex;flex-wrap:wrap;align-items:center;gap:8px;margin:8px 0';
        const status = document.createElement('span');
        status.setAttribute('role', 'status');
        const button = (label, click) => {
            const item = document.createElement('button');
            item.type = 'button';
            item.className = 'btn cbi-button';
            item.textContent = label;
            item.onclick = click;
            actions.appendChild(item);
            return item;
        };
        const copyFallback = (text) => {
            const input = document.createElement('textarea');
            input.value = text;
            input.style.cssText = 'position:fixed;left:-9999px;top:0';
            container.appendChild(input);
            input.focus();
            input.select();
            try {
                if (!document.execCommand('copy')) throw new Error('copy unavailable');
            } finally {
                input.remove();
            }
        };
        button('复制日志', async () => {
            const text = logText();
            try {
                if (navigator.clipboard && window.isSecureContext) {
                    try { await navigator.clipboard.writeText(text); }
                    catch (_) { copyFallback(text); }
                } else {
                    copyFallback(text);
                }
                status.textContent = '日志已复制';
            } catch (_) {
                status.textContent = '复制失败，请使用下载日志';
            }
        });
        button('下载日志', () => {
            const url = URL.createObjectURL(new Blob([logText()], {type:'text/plain;charset=utf-8'}));
            const link = document.createElement('a');
            link.href = url;
            link.download = 'istore-install.log';
            container.appendChild(link);
            link.click();
            link.remove();
            setTimeout(() => URL.revokeObjectURL(url), 1000);
        });
        actions.appendChild(status);
        container.querySelector('.dialog-title-bar').after(actions);

        return {term, container,
            writeLog(data) {
                logParts.push(typeof data === 'string' ? data : decoder.decode(data, {stream:true}));
                term.write(data);
            },
            resetLog() {
                logParts = [];
                decoder = new TextDecoder();
                term.reset();
            }
        };
    };
    const show_log_txt = function(title, content, onclose) {
        const dialog = create_dialog({title, convertEol:true, onhine:onclose});
        const container = dialog.container;
        const term = dialog.term;
        container.querySelector(".dialog-icon-close").hidden = true;
        dialog.writeLog(content);
    };
    const show_log = function(task_id, nohide, onExit) {
        let showing = true;
        let running = true;
        const dialog = create_dialog({title:task_id, nohide, onhide:function(){showing=false;}});
        const container = dialog.container;
        const term = dialog.term;

        const title_view = container.querySelector(".dialog-title-bar .dialog-title");
        container.querySelector(".dialog-icon-close").onclick = function(){
            if (!running || confirm($gettext("Stop running task?"))) {
                running=false;
                showing=false;
                del_task(task_id).then(()=>{
                    location.href = location.href.replace(/#.*$/,'');
                });
            }
            return false;
        };
        const checkTask = function() {
            if (!showing) {
                return Promise.resolve(false);
            }
            return getTaskDetail(task_id).then(data=>{
                if (!running) {
                    return false;
                }
                running = data.running;
                let title = task_id;
                if (!data.running && data.stop) {
                    title += " (" + (data.exit_code?$gettext("Failed at:"):$gettext("Finished at:")) + " " + new Date(data.stop * 1000).toLocaleString() + ")";
                }
                title += " > " + (data.command || '');
                title_view.title = title;
                title_view.innerText = title;
                if (!data.running) {
                    container.classList.add('tasks_stopped');
                    if (data.exit_code) {
                        container.classList.add('tasks_failed');
                    }
                    onExit && onExit(data.exit_code);
                }
                // last pull
                return showing;
            });
        };
        let logoffset = 0;
        const pulllog = function(check) {
            let starter = Promise.resolve(showing);
            if (check) {
                starter = checkTask();
            }
            starter.then(again => {
                if (again)
                    return getBin("/cgi-bin/luci/admin/system/tasks/log?task_id="+task_id+"&offset="+logoffset);
                else
                    return {status: 204};
            }).then(function(res){
                if (!showing) {
                    return false;
                }
                switch(res.status){
                    case 205:
                        dialog.resetLog();
                        logoffset = 0;
                        return running;
                        break;
                    case 204:
                        return running && checkTask();
                        break;
                    case 200:
                        logoffset += res.buffer.byteLength;
                        dialog.writeLog(res.buffer);
                        return running;
                        break;
                }
            }).then(again => {
                if (again) {
                    setTimeout(pulllog, 0);
                }
            }).catch(err => {
                if (showing) {
                    console.error(err);
                    if (err.target) {
                        if (err.target.status == 0 || err.target.status == 502) {
                            title_view.innerText = task_id + ' (' + $gettext("Fetch log failed, retrying...") + ')';
                        } else if (err.target.status == 403 || err.target.status == 404) {
                            title_view.innerText = task_id + ' (' + $gettext(err.target.status == 403?"Lost login status":"Task does not exist or has been deleted") + ')';
                            container.querySelector(".dialog-icon-close").hidden = true;
                            container.classList.add('tasks_unknown');
                            return
                        }
                    }
                    setTimeout(()=>pulllog(true), 1000);
                }
            });
        };
        pulllog(true);
    };
    const del_task = function(task_id) {
        return request("/cgi-bin/luci/admin/system/tasks/stop?task_id="+task_id, "POST");
    };
    taskd.show_log = show_log;
    taskd.remove = del_task;
    taskd.show_log_txt = show_log_txt;
    window.taskd=taskd;
})();

(function(){
    // compat
    if (typeof(window.findParent) !== 'function') {
        const elem = function(e) {
            return (e != null && typeof(e) == 'object' && 'nodeType' in e);
        };
        const matches = function(node, selector) {
            var m = elem(node) ? node.matches || node.msMatchesSelector : null;
            return m ? m.call(node, selector) : false;
        };
        window.findParent = function (node, selector) {
            if (elem(node) && node.closest)
                return node.closest(selector);

            while (elem(node))
                if (matches(node, selector))
                    return node;
                else
                    node = node.parentNode;

            return null;
        };
    }
    if (typeof(window.cbi_submit) !== 'function') {
        const makeHidden = function(name) {
            const input = document.createElement('input');
            input.type = 'hidden';
            input.name = name;
            return input;
        };
        window.cbi_submit = function(elem, name, value, action) {
            var form = elem.form || findParent(elem, 'form');

            if (!form)
                return false;

            if (action)
                form.action = action;

            if (name) {
                var hidden = form.querySelector('input[type="hidden"][name="%s"]'.format(name)) ||
                    makeHidden(name);

                hidden.value = value || '1';
                form.appendChild(hidden);
            }

            form.submit();
            return true;
        };
    }
})();
