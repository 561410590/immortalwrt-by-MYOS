
(function(){function showMsgOverlay(msg,duration){if(document.getElementById('luci-msg-overlay'))return;let overlay=document.createElement('div');overlay.id='luci-msg-overlay';overlay.style=`
            position: fixed; z-index: 9999; left:0; top:0;right:0;bottom:0;
            background: rgba(0,0,0,0.5); color: #fff;
            display: flex; align-items:center; justify-content: center;
            font-size: 2em; opacity:0; transition:opacity 0.5s;
        `;overlay.innerHTML=`<div style="background: rgba(255,255,255,0.85); 
            color: #222; border-radius:12px; padding:2em 3em;
            box-shadow:0 8px 24px #0002; font-size:1.5em;">${msg}</div>`;document.body.appendChild(overlay);setTimeout(()=>{overlay.style.opacity=1;},20);setTimeout(()=>{overlay.style.opacity=0;setTimeout(()=>{overlay.remove();},500);},duration||3000);}
function pollMsg(){XHR.get('<%=luci.dispatcher.build_url("admin", "fancontrol", "msg")%>',{set:'msg',p:'1'},function(x,rv){var msg=rv&&rv.lucimsg;if(msg){showMsgOverlay(msg,3000);setTimeout(pollMsg,3500);}else{setTimeout(pollMsg,2000);}});}
window.addEventListener('DOMContentLoaded',pollMsg);})();