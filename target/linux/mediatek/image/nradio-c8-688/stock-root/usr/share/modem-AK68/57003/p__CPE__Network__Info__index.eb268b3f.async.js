"use strict";(self.webpackChunkant_design_pro=self.webpackChunkant_design_pro||[]).push([[92],{59650:function(Er,Ze,k){k.r(Ze);var En=k(97857),o=k.n(En),Mn=k(15009),h=k.n(Mn),Cn=k(99289),A=k.n(Cn),In=k(5574),j=k.n(In),Ee=k(59955),zn=k(19693),Z=k(2453),Rn=k(41730),be=k(37804),W=k(71230),f=k(15746),T=k(36039),ce=k(77683),Pn=k(66309),An=k(72269),H=k(55054),Bn=k(78957),S=k(67294),e=k(85893),Zn=function(){var Tn=(0,S.useState)("\u7B49\u5F85\u72B6\u6001\u4E2D"),Te=j()(Tn,2),Me=Te[0],ee=Te[1],Nn=(0,S.useState)(!1),Ne=j()(Nn,2),Cr=Ne[0],ae=Ne[1],Ln=(0,S.useState)("\u672A\u77E5\u8FD0\u8425\u5546"),Le=j()(Ln,2),_n=Le[0],pe=Le[1],On=(0,S.useState)(!1),_e=j()(On,2),Ir=_e[0],zr=_e[1],Wn=(0,S.useState)({networkSpeed:{enabled:!1,interval:5},flowStats:{enabled:!1,interval:5},networkInfo:{enabled:!1,interval:5},tempMonitor:{enabled:!1,interval:5}}),Oe=j()(Wn,2),N=Oe[0],J=Oe[1],Hn=(0,S.useState)({networkSpeed:null,flowStats:null,networkInfo:null,tempMonitor:null}),We=j()(Hn,2),D=We[0],q=We[1],Un=(0,S.useState)({networkSpeed:0,flowStats:0,networkInfo:0,tempMonitor:0}),He=j()(Un,2),Rr=He[0],Pr=He[1],Gn=(0,S.useState)({stat:0,lac:"",ci:"",act:-1}),Ue=j()(Gn,2),Ar=Ue[0],Kn=Ue[1],$n=(0,S.useState)(null),Ge=j()($n,2),B=Ge[0],Ce=Ge[1],Qn=(0,S.useState)(!1),Ke=j()(Qn,2),fe=Ke[0],Ie=Ke[1],qn=(0,S.useState)(500),$e=j()(qn,2),Yn=$e[0],Vn=$e[1],Jn=(0,S.useState)(!1),Qe=j()(Jn,2),Xn=Qe[0],ze=Qe[1],er=(0,S.useState)(500),qe=j()(er,2),Re=qe[0],Ye=qe[1],nr=(0,S.useState)(null),Ve=j()(nr,2),L=Ve[0],rr=Ve[1],ar=(0,S.useState)(!1),Je=j()(ar,2),Br=Je[0],Se=Je[1],tr=(0,S.useState)(0),Xe=j()(tr,2),Zr=Xe[0],sr=Xe[1],ir=(0,S.useState)(null),en=j()(ir,2),nn=en[0],Tr=en[1],lr=(0,S.useState)(!1),rn=j()(lr,2),we=rn[0],or=rn[1],dr=function(){var c=A()(h()().mark(function r(a){var l;return h()().wrap(function(n){for(;;)switch(n.prev=n.next){case 0:if(!a){n.next=5;break}Ye(Yn),ze(!0),n.next=15;break;case 5:return n.prev=5,n.next=8,E.setPDCPDataReport(!1);case 8:l=n.sent,l.success?(Ie(!1),Ce(null),Z.ZP.success("\u5173\u95ED\u5B9E\u65F6\u7F51\u901F\u6210\u529F")):Z.ZP.error("\u5173\u95ED\u5B9E\u65F6\u7F51\u901F\u5931\u8D25"),n.next=15;break;case 12:n.prev=12,n.t0=n.catch(5),Z.ZP.error("\u8BBE\u7F6EPDCP\u6570\u636E\u4E0A\u62A5\u5931\u8D25");case 15:case"end":return n.stop()}},r,null,[[5,12]])}));return function(a){return c.apply(this,arguments)}}(),ur=function(){var c=A()(h()().mark(function r(){var a;return h()().wrap(function(s){for(;;)switch(s.prev=s.next){case 0:return s.prev=0,s.next=3,E.setPDCPDataReport(!0,Re);case 3:a=s.sent,a.success?(Ie(!0),Vn(Re),Z.ZP.success("\u5B9E\u65F6\u7F51\u901F\u5F00\u542F\u6210\u529F"),ze(!1)):Z.ZP.error("\u5B9E\u65F6\u7F51\u901F\u5F00\u542F\u5931\u8D25"),s.next=10;break;case 7:s.prev=7,s.t0=s.catch(0),Z.ZP.error("\u5B9E\u65F6\u7F51\u901F\u5F00\u542F\u5931\u8D25");case 10:case"end":return s.stop()}},r,null,[[0,7]])}));return function(){return c.apply(this,arguments)}}();(0,S.useEffect)(function(){var c=function(a){if(fe&&a.type==="pdcp_data"&&"data"in a){var l=a.data;(l.ulPdcpRate>0||l.dlPdcpRate>0)&&rr(l),Ce(l)}};return E.subscribe(c),function(){E.unsubscribe(c),fe&&E.setPDCPDataReport(!1).then(function(){Ie(!1),Ce(null)}).catch(function(r){console.error("\u5173\u95EDPDCP\u6570\u636E\u4E0A\u62A5\u5931\u8D25:",r)})}},[fe]);var cr=function(){return(0,e.jsx)(e.Fragment,{children:(0,e.jsx)(Rn.Z,{title:"\u4E3B\u52A8\u5237\u65B0\u65F6\u95F4",open:Xn,onOk:ur,onCancel:function(){return ze(!1)},destroyOnClose:!0,children:(0,e.jsxs)("div",{style:{padding:"20px 0"},children:[(0,e.jsx)("div",{style:{marginBottom:"10px",color:"#666"},children:"\u4E3B\u52A8\u5237\u65B0\u65F6\u95F4\uFF08200-65535ms\uFF09\uFF1A"}),(0,e.jsx)(be.Z,{min:200,max:65535,step:100,value:Re,onChange:function(a){return a&&Ye(a)},addonAfter:"ms",style:{width:"100%"}}),(0,e.jsx)("div",{style:{marginTop:"10px",color:"#666",fontSize:"12px"},children:"\u8BF4\u660E\uFF1A\u95F4\u9694\u8D8A\u5C0F\uFF0C\u6570\u636E\u66F4\u65B0\u8D8A\u9891\u7E41\uFF0C\u4F46\u7CFB\u7EDF\u8D1F\u62C5\u8D8A\u5927"})]})})})},te=function(r,a,l){if(r==="networkInfo"){if(D.networkInfo&&(clearInterval(D.networkInfo),q(function(d){return o()(o()({},d),{},{networkInfo:null})}),Se(!1)),a){var s=function(){var d=A()(h()().mark(function F(){return h()().wrap(function(u){for(;;)switch(u.prev=u.next){case 0:return u.prev=0,Se(!0),u.next=4,ye();case 4:u.next=13;break;case 6:u.prev=6,u.t0=u.catch(0),console.error("\u5237\u65B0\u7F51\u7EDC\u4FE1\u606F\u5931\u8D25:",u.t0),Z.ZP.error("\u83B7\u53D6\u7F51\u7EDC\u4FE1\u606F\u5931\u8D25"),J(function(m){return o()(o()({},m),{},{networkInfo:o()(o()({},m.networkInfo),{},{enabled:!1})})}),D.networkInfo&&(clearInterval(D.networkInfo),q(function(m){return o()(o()({},m),{},{networkInfo:null})})),Se(!1);case 13:case"end":return u.stop()}},F,null,[[0,6]])}));return function(){return d.apply(this,arguments)}}();s();var n=setInterval(s,l*1e3);q(function(d){return o()(o()({},d),{},{networkInfo:n})})}J(function(d){return o()(o()({},d),{},{networkInfo:{enabled:a,interval:l}})})}else if(r==="flowStats"){if(D.flowStats&&(clearInterval(D.flowStats),q(function(d){return o()(o()({},d),{},{flowStats:null})})),a){var g=function(){var d=A()(h()().mark(function F(){return h()().wrap(function(u){for(;;)switch(u.prev=u.next){case 0:return u.prev=0,u.next=3,me();case 3:u.next=11;break;case 5:u.prev=5,u.t0=u.catch(0),console.error("\u5237\u65B0\u6D41\u91CF\u7EDF\u8BA1\u5931\u8D25:",u.t0),Z.ZP.error("\u5237\u65B0\u6D41\u91CF\u7EDF\u8BA1\u5931\u8D25"),J(function(m){return o()(o()({},m),{},{flowStats:o()(o()({},m.flowStats),{},{enabled:!1})})}),D.flowStats&&(clearInterval(D.flowStats),q(function(m){return o()(o()({},m),{},{flowStats:null})}));case 11:case"end":return u.stop()}},F,null,[[0,5]])}));return function(){return d.apply(this,arguments)}}();g();var i=setInterval(g,l*1e3);q(function(d){return o()(o()({},d),{},{flowStats:i})})}J(function(d){return o()(o()({},d),{},{flowStats:{enabled:a,interval:l}})})}else if(r==="networkSpeed"){if(D.networkSpeed&&(clearInterval(D.networkSpeed),q(function(d){return o()(o()({},d),{},{networkSpeed:null})})),a){var t=function(){var d=A()(h()().mark(function F(){return h()().wrap(function(u){for(;;)switch(u.prev=u.next){case 0:return u.prev=0,u.next=3,me();case 3:u.next=11;break;case 5:u.prev=5,u.t0=u.catch(0),console.error("\u5237\u65B0\u7F51\u901F\u6570\u636E\u5931\u8D25:",u.t0),Z.ZP.error("\u5237\u65B0\u7F51\u901F\u6570\u636E\u5931\u8D25"),J(function(m){return o()(o()({},m),{},{networkSpeed:o()(o()({},m.networkSpeed),{},{enabled:!1})})}),D.networkSpeed&&(clearInterval(D.networkSpeed),q(function(m){return o()(o()({},m),{},{networkSpeed:null})}));case 11:case"end":return u.stop()}},F,null,[[0,5]])}));return function(){return d.apply(this,arguments)}}();t();var w=setInterval(t,l*1e3);q(function(d){return o()(o()({},d),{},{networkSpeed:w})})}J(function(d){return o()(o()({},d),{},{networkSpeed:{enabled:a,interval:l}})})}else if(r==="tempMonitor"){if(D.tempMonitor&&(clearInterval(D.tempMonitor),q(function(d){return o()(o()({},d),{},{tempMonitor:null})})),a){var z=function(){var d=A()(h()().mark(function F(){return h()().wrap(function(u){for(;;)switch(u.prev=u.next){case 0:return u.prev=0,u.next=3,kr();case 3:u.next=11;break;case 5:u.prev=5,u.t0=u.catch(0),console.error("\u5237\u65B0\u6E29\u5EA6\u6570\u636E\u5931\u8D25:",u.t0),Z.ZP.error("\u5237\u65B0\u6E29\u5EA6\u6570\u636E\u5931\u8D25"),J(function(m){return o()(o()({},m),{},{tempMonitor:o()(o()({},m.tempMonitor),{},{enabled:!1})})}),D.tempMonitor&&(clearInterval(D.tempMonitor),q(function(m){return o()(o()({},m),{},{tempMonitor:null})}));case 11:case"end":return u.stop()}},F,null,[[0,5]])}));return function(){return d.apply(this,arguments)}}();z();var R=setInterval(z,l*1e3);q(function(d){return o()(o()({},d),{},{tempMonitor:R})})}J(function(d){return o()(o()({},d),{},{tempMonitor:{enabled:a,interval:l}})})}},an={1:"2100 MHz (FDD)",2:"1900 MHz (FDD)",3:"1800 MHz (FDD)",5:"850 MHz (FDD)",7:"2600 MHz (FDD)",8:"900 MHz (FDD)",20:"800 MHz (FDD)",28:"700 MHz (FDD)",38:"2600 MHz (TDD)",40:"2300 MHz (TDD)",41:"2500 MHz (TDD)",77:"3700 MHz (TDD)",78:"3500 MHz (TDD)",79:"4700 MHz (TDD)"},tn={1:"2100 MHz (FDD)",2:"1900 MHz (FDD)",3:"1800 MHz (FDD)",5:"850 MHz (FDD)",7:"2600 MHz (FDD)",8:"900 MHz (FDD)",20:"800 MHz (FDD)",38:"2600 MHz (TDD)",40:"2300 MHz (TDD)",41:"2500 MHz (TDD)"},ye=function(){var c=A()(h()().mark(function r(){var a,l,s,n,g,i,t,w,z,R,d,F,v,u,m,U;return h()().wrap(function(b){for(;;)switch(b.prev=b.next){case 0:return b.prev=0,ae(!0),b.next=4,E.sendCommand("AT^MONSC");case 4:return a=b.sent,l={},a.success&&a.data&&(s=a.data,s.includes("^MONSC:")?(g=s.replace(/^\^MONSC:\s*/,""),n=g.split(",")):n=s.split(","),n&&n.length>=9&&(l={mcc:n[1],mnc:n[2],channel:n[3],cid:parseInt(n[5],16).toString(),pci:parseInt(n[6],16),lac:parseInt(n[7],16).toString(),rscp:parseInt(n[8],10),signalPercent:re(parseInt(n[8],10)),ecio:parseFloat(n[9])})),b.next=9,E.sendCommand("AT^HFREQINFO?");case 9:if(i=b.sent,t=[],i.success&&i.data&&(w=i.data.split(`
`),w.forEach(function(M){if(M.startsWith("^HFREQINFO:")){var C=M.replace(/^\^HFREQINFO:\s*/,"").split(",");if(C.length>=9)for(var I=C[1],$=2,x=I==="7"?3:4;$+6<=C.length&&t.length<x;){var _=parseInt(C[$]),he=I==="7"?"n".concat(_):"B".concat(_);t.push({band:_.toString(),bandShortName:he,bandDesc:I==="7"?an[_.toString()]||"\u672A\u77E5\u9891\u6BB5":tn[_.toString()]||"\u672A\u77E5\u9891\u6BB5",dlFcn:C[$+1].trim(),dlFreq:(parseInt(C[$+2])*(I==="7"?.001:.1)).toFixed(2),dlBandwidth:parseInt(C[$+3])/1e3,ulFcn:C[$+4].trim(),ulFreq:(parseInt(C[$+5])*(I==="7"?.001:.1)).toFixed(2),ulBandwidth:parseInt(C[$+6])/1e3,sysMode:I==="7"?"NR":"LTE"}),$+=7}}})),z=parseFloat(t.reduce(function(M,C){return M+C.dlBandwidth},0).toFixed(2)),R=parseFloat(t.reduce(function(M,C){return M+C.ulBandwidth},0).toFixed(2)),d="",!(t.length>0)){b.next=35;break}if(!(t.some(function(M){return M.sysMode==="NR"})&&t.some(function(M){return M.sysMode==="LTE"}))){b.next=20;break}d="EN-DC (LTE+NR)",b.next=33;break;case 20:if(!t.some(function(M){return M.sysMode==="NR"})){b.next=24;break}d=t.length>1?"NR-CA":"NR",b.next=33;break;case 24:if(!t.some(function(M){return M.sysMode==="LTE"})){b.next=28;break}d=t.length>1?"LTE-CA":"LTE",b.next=33;break;case 28:return b.next=30,E.sendCommand("AT+HCSQ?");case 30:v=b.sent,u=v==null||(F=v.data)===null||F===void 0||(F=F.split(",")[0])===null||F===void 0?void 0:F.replace(/"/g,""),u==="NR"?d="NR":u==="LTE"?d="LTE":u==="WCDMA"?d="WCDMA":d="\u672A\u77E5";case 33:b.next=37;break;case 35:U=(m=hcsqResponse)===null||m===void 0||(m=m.data)===null||m===void 0||(m=m.split(",")[0])===null||m===void 0?void 0:m.replace(/"/g,""),U==="NR"?d="NR":U==="LTE"?d="LTE":U==="WCDMA"?d="WCDMA":d="\u672A\u77E5";case 37:ne(function(M){return o()(o()(o()({},M),l),{},{carrierInfo:t,carrierCount:t.length,dlBandwidth:z,ulBandwidth:R,networkMode:d,sysMode:d})}),b.next=43;break;case 40:b.prev=40,b.t0=b.catch(0),Z.ZP.error("\u83B7\u53D6\u7F51\u7EDC\u4FE1\u606F\u5931\u8D25");case 43:return b.prev=43,ae(!1),b.finish(43);case 46:case"end":return b.stop()}},r,null,[[0,40,43,46]])}));return function(){return c.apply(this,arguments)}}(),pr=(0,S.useState)({rscp:0,signalPercent:"",ecio:0,sinr:0,mcc:"",mnc:"",lac:"",cid:"",channel:"",band:"",dlBandwidth:0,ulBandwidth:0,pci:0,carrierInfo:[],carrierCount:0,networkMode:"",sysMode:"\u672A\u77E5"}),sn=j()(pr,2),p=sn[0],ne=sn[1],fr=(0,S.useState)({sub3GPA:0,sub6GPA:0,mimoPa:0,tcxo:0,peri1:0,peri2:0,ap1:0,ap2:0,modem1:0,modem2:0,bbp1:0,bbp2:0}),ln=j()(fr,2),P=ln[0],on=ln[1],E=Ee.S.getInstance(),xr=(0,S.useState)(""),dn=j()(xr,2),mr=dn[0],hr=dn[1],gr=(0,S.useState)(0),un=j()(gr,2),Pe=un[0],vr=un[1],br=(0,S.useState)(0),cn=j()(br,2),Ae=cn[0],Sr=cn[1],pn=function(){var c=A()(h()().mark(function r(){var a,l,s,n,g;return h()().wrap(function(t){for(;;)switch(t.prev=t.next){case 0:return t.prev=0,t.next=3,E.sendCommand("AT^DSAMBR=1");case 3:if(a=t.sent,!(!a.success||!a.data)){t.next=8;break}return t.next=7,E.sendCommand("AT^DSAMBR=8");case 7:a=t.sent;case 8:a.success&&a.data&&(l=a.data.split(","),l.length>=4&&(s=l[3].trim(),hr(s.substring(1,s.length-1)),n=parseInt(l[1])/1e3,g=parseInt(l[2])/1e3,vr(n),Sr(g))),t.next=14;break;case 11:t.prev=11,t.t0=t.catch(0),Z.ZP.error("\u83B7\u53D6 AMBR \u4FE1\u606F\u5931\u8D25");case 14:case"end":return t.stop()}},r,null,[[0,11]])}));return function(){return c.apply(this,arguments)}}(),wr=(0,S.useState)("\u672A\u77E5"),fn=j()(wr,2),xn=fn[0],Y=fn[1],mn=function(){var c=A()(h()().mark(function r(){var a,l,s,n;return h()().wrap(function(i){for(;;)switch(i.prev=i.next){case 0:return i.prev=0,i.next=3,E.sendCommand("AT+CGEQOSRDP=8");case 3:if(a=i.sent,!(!a.success||!a.data)){i.next=8;break}return i.next=7,E.sendCommand("AT+CGEQOSRDP=1");case 7:a=i.sent;case 8:if(!(a.success&&a.data)){i.next=38;break}if(l=a.data,s=l.match(/\+CGEQOSRDP:\s*\d+,(\d+)/),!(s&&s[1])){i.next=37;break}n=s[1],i.t0=n,i.next=i.t0==="1"?16:i.t0==="2"?18:i.t0==="3"?20:i.t0==="4"?22:i.t0==="5"?24:i.t0==="6"?26:i.t0==="7"?28:i.t0==="8"?30:i.t0==="9"?32:34;break;case 16:return Y("\u7B49\u7EA71\uFF1AGBR\u4E1A\u52A1,\u5EF6\u8FDF100ms,\u4E22\u5305\u738710^-2,\u9AD8\u4F18\u5148\u7EA7\u8BED\u97F3\u901A\u8BDD"),i.abrupt("break",35);case 18:return Y("\u7B49\u7EA72\uFF1AGBR\u4E1A\u52A1,\u5EF6\u8FDF150ms,\u4E22\u5305\u738710^-3,\u6807\u51C6\u8BED\u97F3\u901A\u8BDD"),i.abrupt("break",35);case 20:return Y("\u7B49\u7EA73\uFF1AGBR\u4E1A\u52A1,\u5EF6\u8FDF50ms,\u4E22\u5305\u738710^-3,\u5B9E\u65F6\u6E38\u620F"),i.abrupt("break",35);case 22:return Y("\u7B49\u7EA74\uFF1AGBR\u4E1A\u52A1,\u5EF6\u8FDF300ms,\u4E22\u5305\u738710^-6,\u975E\u4F1A\u8BDD\u89C6\u9891"),i.abrupt("break",35);case 24:return Y("\u7B49\u7EA75\uFF1A\u975EGBR\u4E1A\u52A1,\u5EF6\u8FDF100ms,\u4E22\u5305\u738710^-6,IMS\u4FE1\u4EE4"),i.abrupt("break",35);case 26:return Y("\u7B49\u7EA76\uFF1A\u975EGBR\u4E1A\u52A1,\u5EF6\u8FDF300ms,\u4E22\u5305\u738710^-6,\u89C6\u9891\u6D41\u5A92\u4F53"),i.abrupt("break",35);case 28:return Y("\u7B49\u7EA77\uFF1A\u975EGBR\u4E1A\u52A1,\u5EF6\u8FDF100ms,\u4E22\u5305\u738710^-3,\u8BED\u97F3\u3001\u89C6\u9891\u3001\u4E92\u52A8\u6E38\u620F"),i.abrupt("break",35);case 30:return Y("\u7B49\u7EA78\uFF1A\u975EGBR\u4E1A\u52A1,\u5EF6\u8FDF300ms,\u4E22\u5305\u738710^-6,\u89C6\u9891\u6D41\u5A92\u4F53\u3001TCP\u5E94\u7528"),i.abrupt("break",35);case 32:return Y("\u7B49\u7EA79\uFF1A\u975EGBR\u4E1A\u52A1,\u5EF6\u8FDF300ms,\u4E22\u5305\u738710^-6,\u6807\u51C6\u6570\u636E\u4F20\u8F93"),i.abrupt("break",35);case 34:Y("QCI ".concat(n,"\uFF1A\u672A\u77E5\u670D\u52A1\u7B49\u7EA7"));case 35:i.next=38;break;case 37:Y("\u672A\u80FD\u83B7\u53D6\u670D\u52A1\u7B49\u7EA7\u4FE1\u606F");case 38:i.next=43;break;case 40:i.prev=40,i.t1=i.catch(0),Z.ZP.error("\u83B7\u53D6\u670D\u52A1\u7B49\u7EA7\u4FE1\u606F\u5931\u8D25");case 43:case"end":return i.stop()}},r,null,[[0,40]])}));return function(){return c.apply(this,arguments)}}(),yr=(0,S.useState)({lastDsTime:0,lastTxFlow:0,lastRxFlow:0,totalDsTime:0,totalTxFlow:0,totalRxFlow:0}),hn=j()(yr,2),se=hn[0],Fr=hn[1],jr=(0,S.useState)({upSpeed:0,downSpeed:0,lastUpdateTime:0,lastTxFlow:0,lastRxFlow:0}),gn=j()(jr,2),xe=gn[0],vn=gn[1],ie=function(r){return parseInt(r,16)},Fe=function(r){return r<1024?"".concat(r," B"):r<1024*1024?"".concat((r/1024).toFixed(2)," KB"):r<1024*1024*1024?"".concat((r/(1024*1024)).toFixed(2)," MB"):"".concat((r/(1024*1024*1024)).toFixed(2)," GB")},je=function(r){var a=r*8;return a>=1e9?"".concat((a/1e9).toFixed(2)," Gbps"):a>=1e6?"".concat((a/1e6).toFixed(2)," Mbps"):a>=1e3?"".concat((a/1e3).toFixed(2)," Kbps"):"".concat(Math.round(a)," bps")},bn=function(r){if(we){var a=Math.floor(r/86400),l=Math.floor(r%86400/3600),s=Math.floor(r%3600/60),n=r%60;return"".concat(a,"\u5929").concat(l,"\u65F6").concat(s,"\u5206").concat(n,"\u79D2")}else{var g=Math.floor(r/3600),i=Math.floor(r%3600/60),t=r%60;return"".concat(g,"\u65F6").concat(i,"\u5206").concat(t,"\u79D2")}},me=function(){var c=A()(h()().mark(function r(){var a,l,s,n,g,i,t,w,z,R,d;return h()().wrap(function(v){for(;;)switch(v.prev=v.next){case 0:return v.prev=0,v.next=3,E.sendCommand("AT^DSFLOWQRY");case 3:a=v.sent,a.success&&a.data&&(l=a.data.replace(/^\^DSFLOWQRY:\s*/,""),s=l.split(","),s.length>=6&&(n=Date.now(),g=ie(s[4]),i=ie(s[5]),xe.lastUpdateTime>0?(t=(n-xe.lastUpdateTime)/1e3,t>0&&(w=g-xe.lastTxFlow,z=i-xe.lastRxFlow,R=w/t,d=z/t,vn({upSpeed:R,downSpeed:d,lastUpdateTime:n,lastTxFlow:g,lastRxFlow:i}))):vn(o()(o()({},xe),{},{lastUpdateTime:n,lastTxFlow:g,lastRxFlow:i})),Fr({lastDsTime:ie(s[0]),lastTxFlow:ie(s[1]),lastRxFlow:ie(s[2]),totalDsTime:ie(s[3]),totalTxFlow:g,totalRxFlow:i}))),v.next=10;break;case 7:v.prev=7,v.t0=v.catch(0),Z.ZP.error("\u83B7\u53D6\u6D41\u91CF\u7EDF\u8BA1\u4FE1\u606F\u5931\u8D25");case 10:case"end":return v.stop()}},r,null,[[0,7]])}));return function(){return c.apply(this,arguments)}}(),Dr=function(){var c=A()(h()().mark(function r(){var a;return h()().wrap(function(s){for(;;)switch(s.prev=s.next){case 0:return s.prev=0,s.next=3,E.sendCommand("AT^DSFLOWCLR");case 3:a=s.sent,a.success?(Z.ZP.success("\u6D41\u91CF\u7EDF\u8BA1\u5DF2\u6E05\u96F6"),me()):Z.ZP.error("\u6D41\u91CF\u7EDF\u8BA1\u6E05\u96F6\u5931\u8D25"),s.next=10;break;case 7:s.prev=7,s.t0=s.catch(0),Z.ZP.error("\u6D41\u91CF\u7EDF\u8BA1\u6E05\u96F6\u5931\u8D25");case 10:case"end":return s.stop()}},r,null,[[0,7]])}));return function(){return c.apply(this,arguments)}}(),Sn=function(){var c=A()(h()().mark(function r(){var a,l,s,n,g,i,t,w,z,R,d,F,v,u,m,U,X,b,M,C,I;return h()().wrap(function(x){for(;;)switch(x.prev=x.next){case 0:return x.prev=0,ae(!0),x.next=4,E.sendCommand("AT^HCSQ?");case 4:return a=x.sent,a.success&&a.data&&(l=a.data.split(`
`),s=null,n=null,l.forEach(function(_){var he=_.replace(/^\^HCSQ:\s*/,""),ge=he.split(","),G=ge[0].replace(/"/g,"");G==="LTE"?s=ge:G==="NR"&&(n=ge)}),n?(g=parseInt(n[1]),!isNaN(g)&&g!==255&&(i=g===0?-140:g>=97?-44:-140+g,t=re(i),w=n.length>2?parseInt(n[2]):255,z=0,w!==255&&!isNaN(w)&&(z=w===0?-20:w>=251?30:-20+w*.2,z=Math.min(30,Math.max(-20,z))),ne(function(_){return o()(o()({},_),{},{rscp:i,signalPercent:t,sinr:Math.round(z),sysMode:"NR"})}))):s&&(R=parseInt(s[1]),!isNaN(R)&&R!==255&&(d=R===0?-140:R>=97?-44:-140+R,F=re(d),v=s.length>3?parseInt(s[3]):255,u=0,v!==255&&!isNaN(v)&&(u=v===0?-20:v>=251?30:-20+v*.2,u=Math.min(30,Math.max(-20,u))),m=s.length>4?parseInt(s[4]):255,U=m!==255&&!isNaN(m)?m===0?-19.5:m>=34?-3:-19.5+m*.5:0,ne(function(_){return o()(o()({},_),{},{rscp:d,signalPercent:F,sinr:Math.round(u),ecio:Math.round(U),sysMode:"LTE"})})))),x.next=8,E.sendCommand("AT^EONS=2");case 8:if(X=x.sent,!(X.success&&X.data)){x.next=23;break}M=(b=X.data.split(",")[1])===null||b===void 0?void 0:b.trim(),x.t0=M,x.next=x.t0==="46000"||x.t0==="46002"||x.t0==="46004"||x.t0==="46007"||x.t0==="46008"||x.t0==="46020"?14:x.t0==="46001"||x.t0==="46006"||x.t0==="46009"?16:x.t0==="46003"||x.t0==="46005"||x.t0==="46011"?18:x.t0==="46015"?20:22;break;case 14:return pe("\u4E2D\u56FD\u79FB\u52A8"),x.abrupt("break",23);case 16:return pe("\u4E2D\u56FD\u8054\u901A"),x.abrupt("break",23);case 18:return pe("\u4E2D\u56FD\u7535\u4FE1"),x.abrupt("break",23);case 20:return pe("\u4E2D\u56FD\u5E7F\u7535"),x.abrupt("break",23);case 22:pe("\u672A\u77E5\u8FD0\u8425\u5546");case 23:return x.next=25,pn();case 25:return x.next=27,mn();case 27:return x.next=29,me();case 29:return x.next=31,E.sendCommand("AT^CHIPTEMP?");case 31:C=x.sent,C.success&&C.data&&(I=C.data.split(":")[1].trim().split(","),on({sub3GPA:parseFloat((parseInt(I[0])/10).toFixed(1)),sub6GPA:parseFloat((parseInt(I[1])/10).toFixed(1)),mimoPa:parseFloat((parseInt(I[2])/10).toFixed(1)),tcxo:parseFloat((parseInt(I[3])/10).toFixed(1)),peri1:parseFloat((parseInt(I[4])/10).toFixed(1)),peri2:parseFloat((parseInt(I[5])/10).toFixed(1)),ap1:parseFloat((parseInt(I[6])/10).toFixed(1)),ap2:parseFloat((parseInt(I[7])/10).toFixed(1)),modem1:parseFloat((parseInt(I[8])/10).toFixed(1)),modem2:parseFloat((parseInt(I[9])/10).toFixed(1)),bbp1:parseFloat((parseInt(I[10])/10).toFixed(1)),bbp2:parseFloat((parseInt(I[11])/10).toFixed(1))})),x.next=38;break;case 35:x.prev=35,x.t1=x.catch(0),Z.ZP.error("\u83B7\u53D6\u7F51\u7EDC\u72B6\u6001\u5931\u8D25");case 38:return x.prev=38,ae(!1),x.finish(38);case 41:case"end":return x.stop()}},r,null,[[0,35,38,41]])}));return function(){return c.apply(this,arguments)}}(),wn=function(){var c=A()(h()().mark(function r(){var a,l;return h()().wrap(function(n){for(;;)switch(n.prev=n.next){case 0:return n.prev=0,n.next=3,E.getPSRegStatus();case 3:if(a=n.sent,!(a.success&&a.data)){n.next=23;break}l=JSON.parse(a.data),Kn(l),n.t0=l.stat,n.next=n.t0===0?10:n.t0===1?12:n.t0===2?14:n.t0===3?16:n.t0===4?18:n.t0===5?20:22;break;case 10:return ee("\u672A\u641C\u7D22\u7F51\u7EDC"),n.abrupt("break",23);case 12:return ee("\u5DF2\u6CE8\u518C\uFF0C\u672C\u5730\u7F51\u7EDC"),n.abrupt("break",23);case 14:return ee("\u6B63\u5728\u641C\u7D22\u7F51\u7EDC..."),n.abrupt("break",23);case 16:return ee("\u6CE8\u518C\u88AB\u62D2\u7EDD"),n.abrupt("break",23);case 18:return ee("\u672A\u77E5\u72B6\u6001"),n.abrupt("break",23);case 20:return ee("\u5DF2\u6CE8\u518C\uFF0C\u6F2B\u6E38\u7F51\u7EDC"),n.abrupt("break",23);case 22:ee("\u672A\u77E5\u72B6\u6001");case 23:n.next=27;break;case 25:n.prev=25,n.t1=n.catch(0);case 27:case"end":return n.stop()}},r,null,[[0,25]])}));return function(){return c.apply(this,arguments)}}(),Nr=function(){var c=A()(h()().mark(function r(){var a,l,s,n,g,i,t,w,z,R,d,F,v,u,m,U,X,b,M,C,I,$,x,_,he,ge,G,K,le,V,oe,yn,Fn,jn,Dn,Q,de,De,ue,ke;return h()().wrap(function(O){for(;;)switch(O.prev=O.next){case 0:return O.prev=0,O.next=3,E.sendCommand("AT^HCSQ?");case 3:return a=O.sent,a.success&&a.data&&(l=a.data.split(`
`),s=null,n=null,l.forEach(function(y){var ve=y.replace(/^\^HCSQ:\s*/,""),Be=ve.split(","),kn=Be[0].replace(/"/g,"");kn==="LTE"?s=Be:kn==="NR"&&(n=Be)}),n?(g=parseInt(n[1]),!isNaN(g)&&g!==255&&(i=g===0?-140:g>=97?-44:-140+g,t=n.length>2?parseInt(n[2]):255,w=0,t!==255&&!isNaN(t)&&(w=t===0?-20:t>=251?30:-20+t*.2,w=Math.min(30,Math.max(-20,w))),z=n.length>3?parseInt(n[3]):255,R=z!==255&&!isNaN(z)?z===0?-19.5:z>=34?-3:-19.5+z*.5:0,ne(function(y){return o()(o()({},y),{},{rscp:i,signalPercent:re(i),sinr:Math.round(w),sysMode:"NR",networkMode:s?"EN-DC (LTE+NR)":"NR"})}))):s&&(d=parseInt(s[1]),!isNaN(d)&&d!==255&&(F=d===0?-140:d>=97?-44:-140+d,v=re(F),u=s.length>3?parseInt(s[3]):255,m=0,u!==255&&!isNaN(u)&&(m=u===0?-20:u>=251?30:-20+u*.2,m=Math.min(30,Math.max(-20,m))),U=s.length>4?parseInt(s[4]):255,X=U!==255&&!isNaN(U)?U===0?-19.5:U>=34?-3:-19.5+U*.5:0,ne(function(y){return o()(o()({},y),{},{rscp:F,signalPercent:v,sinr:Math.round(m),ecio:Math.round(X),sysMode:"LTE",networkMode:"LTE"})})))),O.next=7,E.sendCommand("AT^MONSC");case 7:if(b=O.sent,!(b.success&&b.data)){O.next=19;break}return M=b.data,M.includes("^MONSC:")?(I=M.replace(/^\^MONSC:\s*/,""),C=I.split(",")):C=M.split(","),$=parseInt(C[8],10),x=re($),O.next=15,E.sendCommand("AT^HFREQINFO?");case 15:if(_=O.sent,he=[],ge="",_.success&&_.data&&(G=_.data.replace(/^\^HFREQINFO:\s*/,"").split(","),K=[],G.length>=9)){for(le=G[1],V=2;V+6<=G.length&&K.length<3;)oe=parseInt(G[V]),yn=le==="7"?"n".concat(oe):"B".concat(oe),Fn=le==="7"?an[oe.toString()]||"\u672A\u77E5\u9891\u6BB5":tn[oe.toString()]||"\u672A\u77E5\u9891\u6BB5",K.push({band:oe.toString(),bandShortName:yn,bandDesc:Fn,dlFcn:G[V+1].trim(),dlFreq:(parseInt(G[V+2])*(le==="7"?.001:.1)).toFixed(2),dlBandwidth:parseInt(G[V+3])/1e3,ulFcn:G[V+4].trim(),ulFreq:(parseInt(G[V+5])*(le==="7"?.001:.1)).toFixed(2),ulBandwidth:parseInt(G[V+6])/1e3,sysMode:le==="7"?"NR":"LTE"}),V+=7;jn=parseFloat(K.reduce(function(y,ve){return y+ve.dlBandwidth},0).toFixed(2)),Dn=parseFloat(K.reduce(function(y,ve){return y+ve.ulBandwidth},0).toFixed(2)),Q="",K.length>0?K.some(function(y){return y.sysMode==="NR"})&&K.some(function(y){return y.sysMode==="LTE"})?Q="EN-DC (LTE+NR)":K.some(function(y){return y.sysMode==="NR"})?Q=K.length>1?"NR-CA":"NR":K.some(function(y){return y.sysMode==="LTE"})?Q=K.length>1?"LTE-CA":"LTE":(De=a==null||(de=a.data)===null||de===void 0||(de=de.split(",")[0])===null||de===void 0?void 0:de.replace(/"/g,""),De==="NR"?Q="NR":De==="LTE"?Q="LTE":De==="WCDMA"?Q="WCDMA":Q="\u672A\u77E5"):(ke=a==null||(ue=a.data)===null||ue===void 0||(ue=ue.split(",")[0])===null||ue===void 0?void 0:ue.replace(/"/g,""),ke==="NR"?Q="NR":ke==="LTE"?Q="LTE":ke==="WCDMA"?Q="WCDMA":Q="\u672A\u77E5"),ne(function(y){return o()(o()({},y),{},{carrierInfo:K,carrierCount:K.length,dlBandwidth:jn,ulBandwidth:Dn,networkMode:Q,mcc:y.mcc,mnc:y.mnc,lac:y.lac,cid:y.cid,channel:y.channel,pci:y.pci,rscp:y.rscp,signalPercent:y.signalPercent,ecio:y.ecio,sinr:y.sinr})})}case 19:O.next=24;break;case 21:O.prev=21,O.t0=O.catch(0),console.error("\u5237\u65B0\u7F51\u7EDC\u4FE1\u606F\u5931\u8D25:",O.t0);case 24:case"end":return O.stop()}},r,null,[[0,21]])}));return function(){return c.apply(this,arguments)}}(),kr=function(){var c=A()(h()().mark(function r(){var a,l;return h()().wrap(function(n){for(;;)switch(n.prev=n.next){case 0:return n.prev=0,n.next=3,E.sendCommand("AT^CHIPTEMP?");case 3:a=n.sent,a.success&&a.data&&(l=a.data.split(":")[1].trim().split(","),on({sub3GPA:parseFloat((parseInt(l[0])/10).toFixed(1)),sub6GPA:parseFloat((parseInt(l[1])/10).toFixed(1)),mimoPa:parseFloat((parseInt(l[2])/10).toFixed(1)),tcxo:parseFloat((parseInt(l[3])/10).toFixed(1)),peri1:parseFloat((parseInt(l[4])/10).toFixed(1)),peri2:parseFloat((parseInt(l[5])/10).toFixed(1)),ap1:parseFloat((parseInt(l[6])/10).toFixed(1)),ap2:parseFloat((parseInt(l[7])/10).toFixed(1)),modem1:parseFloat((parseInt(l[8])/10).toFixed(1)),modem2:parseFloat((parseInt(l[9])/10).toFixed(1)),bbp1:parseFloat((parseInt(l[10])/10).toFixed(1)),bbp2:parseFloat((parseInt(l[11])/10).toFixed(1))})),n.next=11;break;case 7:throw n.prev=7,n.t0=n.catch(0),console.error("\u5237\u65B0\u6E29\u5EA6\u6570\u636E\u5931\u8D25:",n.t0),n.t0;case 11:case"end":return n.stop()}},r,null,[[0,7]])}));return function(){return c.apply(this,arguments)}}();(0,S.useEffect)(function(){var c=function(){var r=A()(h()().mark(function a(){var l;return h()().wrap(function(n){for(;;)switch(n.prev=n.next){case 0:return n.next=2,E.connect();case 2:if(l=n.sent,!l){n.next=20;break}return n.prev=4,ae(!0),n.next=8,wn();case 8:return n.next=10,ye();case 10:return n.next=12,Sn();case 12:n.next=17;break;case 14:n.prev=14,n.t0=n.catch(4),Z.ZP.error("\u521D\u59CB\u5316\u7F51\u7EDC\u4FE1\u606F\u5931\u8D25");case 17:return n.prev=17,ae(!1),n.finish(17);case 20:case"end":return n.stop()}},a,null,[[4,14,17,20]])}));return function(){return r.apply(this,arguments)}}();return c(),function(){Object.values(D).forEach(function(r){r&&clearInterval(r)}),E.disconnect()}},[]);var Lr=function(r){return r>=31?4:r>=21?3:r>=11?2:r>=1?1:0};(0,S.useEffect)(function(){var c=Ee.S.getInstance(),r=function(l){};return c.subscribe(r),function(){c.unsubscribe(r)}},[]),(0,S.useEffect)(function(){var c=Ee.S.getInstance(),r=null,a=function(){var n=A()(h()().mark(function g(i){var t,w;return h()().wrap(function(R){for(;;)switch(R.prev=R.next){case 0:if(!N.networkInfo.enabled){R.next=2;break}return R.abrupt("return");case 2:i.type==="signal_data"&&i.success&&(t=i.data,w={},t.rsrp!==void 0&&(w.rscp=t.rsrp,w.signalPercent=re(t.rsrp)),t.sinr!==void 0&&(w.sinr=t.sinr),t.rsrq!==void 0&&(w.ecio=t.rsrq),t.rssi!==void 0&&(w.rssi=t.rssi),Object.keys(w).length>0&&ne(function(d){return o()(o()({},d),w)}),r&&clearInterval(r),Se(!0),sr(0),setTimeout(A()(h()().mark(function d(){return h()().wrap(function(v){for(;;)switch(v.prev=v.next){case 0:return v.next=2,ye();case 2:case"end":return v.stop()}},d)})),5e3));case 3:case"end":return R.stop()}},g)}));return function(i){return n.apply(this,arguments)}}(),l=function(){var n=A()(h()().mark(function g(){return h()().wrap(function(t){for(;;)switch(t.prev=t.next){case 0:return t.prev=0,t.next=3,ye();case 3:return t.next=5,Sn();case 5:t.next=10;break;case 7:t.prev=7,t.t0=t.catch(0),console.error("\u6570\u636E\u5237\u65B0\u5931\u8D25:",t.t0);case 10:case"end":return t.stop()}},g,null,[[0,7]])}));return function(){return n.apply(this,arguments)}}(),s=function(){var n=A()(h()().mark(function g(){return h()().wrap(function(t){for(;;)switch(t.prev=t.next){case 0:return t.prev=0,t.next=3,l();case 3:return t.next=5,pn();case 5:return t.next=7,mn();case 7:return t.next=9,wn();case 9:return t.next=11,me();case 11:t.next=16;break;case 13:t.prev=13,t.t0=t.catch(0),console.error("\u6570\u636E\u5237\u65B0\u5931\u8D25:",t.t0);case 16:case"end":return t.stop()}},g,null,[[0,13]])}));return function(){return n.apply(this,arguments)}}();return c.subscribe(a),function(){c.unsubscribe(a),r&&clearInterval(r),nn&&clearInterval(nn),D.networkSpeed&&clearInterval(D.networkSpeed),D.flowStats&&clearInterval(D.flowStats),D.networkInfo&&clearInterval(D.networkInfo),D.tempMonitor&&clearInterval(D.tempMonitor)}},[]);var re=function(r){return r>=-80?"100%":r>=-90?"90%":r>=-100?"80%":r>=-110?"50%":"25%"};return(0,e.jsxs)("div",{children:[(0,e.jsxs)(W.Z,{gutter:[16,16],children:[(0,e.jsx)(f.Z,{xs:24,md:24,children:(0,e.jsx)(T.Z,{title:(0,e.jsxs)("div",{style:{display:"flex",alignItems:"center",gap:"8px"},children:[(0,e.jsx)("span",{children:"\u7F51\u7EDC\u4FE1\u606F"}),(0,e.jsx)("div",{style:{fontSize:"12px",color:"#666",background:"#f5f5f5",padding:"2px 8px",borderRadius:"4px",fontWeight:"normal"},children:"\u5C55\u793A\u5F53\u524D\u7F51\u7EDC\u7684\u5404\u9879\u5173\u952E\u6307\u6807"})]}),extra:(0,e.jsxs)(ce.ZP,{type:"link",size:"small",style:{padding:"0 8px",height:"28px",display:"flex",alignItems:"center",gap:"4px",background:N.networkInfo.enabled?"#e6f7ff":"transparent",border:"1px solid #91d5ff",borderRadius:"4px"},onClick:function(r){r.target.closest(".ant-input-number")||te("networkInfo",!N.networkInfo.enabled,N.networkInfo.interval)},children:[(0,e.jsx)("span",{children:"\u81EA\u52A8\u5237\u65B0"}),N.networkInfo.enabled&&(0,e.jsx)(be.Z,{min:1,max:60,value:N.networkInfo.interval,onChange:function(r){return te("networkInfo",!0,r||5)},style:{width:45},size:"small",bordered:!1}),N.networkInfo.enabled&&(0,e.jsx)("span",{children:"\u79D2"})]}),className:"inner-card",children:(0,e.jsxs)(W.Z,{gutter:[16,16],children:[(0,e.jsx)(f.Z,{xs:24,lg:16,children:(0,e.jsx)(T.Z,{size:"small",title:(0,e.jsxs)("div",{style:{display:"flex",alignItems:"center",gap:"8px"},children:["\u4FE1\u53F7\u770B\u677F",p.networkMode&&(0,e.jsx)("span",{style:{fontSize:"13px",backgroundColor:p.networkMode.includes("NR")?"#52c41a":p.networkMode.includes("LTE")?"#1890ff":p.networkMode.includes("WCDMA")?"#faad14":p.networkMode.includes("GSM")?"#ff4d4f":"#999",color:"#fff",padding:"1px 6px",borderRadius:"10px",marginLeft:"8px"},children:p.networkMode}),(0,e.jsx)(Pn.Z,{color:Me.includes("\u672C\u5730")?"success":Me.includes("\u6F2B\u6E38")?"warning":"error",children:Me})]}),bordered:!0,style:{background:"var(--ant-bg-elevated)",height:"100%",border:"1px solid var(--ant-border-color-split)",boxShadow:"0 1px 2px 0 rgba(0, 0, 0, 0.03), 0 1px 6px -1px rgba(0, 0, 0, 0.02), 0 2px 4px 0 rgba(0, 0, 0, 0.02)"},children:(0,e.jsxs)(W.Z,{gutter:[16,16],children:[(0,e.jsx)(f.Z,{xs:6,sm:6,children:(0,e.jsxs)("div",{style:{textAlign:"center"},children:[(0,e.jsx)(zn.Z,{style:{fontSize:"32px",color:p.signalPercent==="100%"||p.signalPercent==="90%"?"#52c41a":p.signalPercent==="80%"?"#faad14":(p.signalPercent==="50%","#ff4d4f")}}),(0,e.jsx)("div",{style:{marginTop:"8px",fontWeight:"bold",color:p.signalPercent==="100%"||p.signalPercent==="90%"?"#52c41a":p.signalPercent==="80%"?"#faad14":(p.signalPercent==="50%","#ff4d4f")},children:p.signalPercent||"\u672A\u77E5"}),(0,e.jsx)("div",{style:{fontSize:"12px",color:"var(--ant-text-color-secondary)"},children:"\u4FE1\u53F7\u8D28\u91CF"})]})}),(0,e.jsx)(f.Z,{xs:6,sm:6,children:(0,e.jsxs)("div",{style:{textAlign:"center"},children:[(0,e.jsx)("div",{style:{fontSize:"24px",fontWeight:"bold",color:p.rscp>=-85?"#52c41a":p.rscp>=-95?"#faad14":"#ff4d4f"},children:p.rscp}),(0,e.jsx)("div",{style:{fontSize:"12px",color:"var(--ant-text-color-secondary)"},children:p.networkMode.includes("NR")||p.networkMode.includes("LTE")?"RSRP (dBm)":p.networkMode.includes("WCDMA")?"RSCP (dBm)":"RSSI (dBm)"}),(0,e.jsx)("div",{style:{fontSize:"12px",color:"var(--ant-text-color-secondary)"},children:p.networkMode.includes("NR")||p.networkMode.includes("LTE")?"\u53C2\u8003\u4FE1\u53F7\u63A5\u6536\u529F\u7387":p.networkMode.includes("WCDMA")?"\u63A5\u6536\u4FE1\u53F7\u7801\u529F\u7387":"\u63A5\u6536\u4FE1\u53F7\u5F3A\u5EA6\u6307\u793A"})]})}),(0,e.jsx)(f.Z,{xs:6,sm:6,children:(0,e.jsxs)("div",{style:{textAlign:"center"},children:[(0,e.jsx)("div",{style:{fontSize:"24px",fontWeight:"bold",color:p.sinr>=20?"#52c41a":p.sinr>=10?"#faad14":"#ff4d4f"},children:p.sinr}),(0,e.jsx)("div",{style:{fontSize:"12px",color:"var(--ant-text-color-secondary)"},children:p.networkMode.includes("NR")||p.networkMode.includes("LTE")?"SINR (dB)":p.networkMode.includes("WCDMA")?"Ec/Io (dB)":"SINR (dB)"}),(0,e.jsx)("div",{style:{fontSize:"12px",color:"var(--ant-text-color-secondary)"},children:p.networkMode.includes("NR")||p.networkMode.includes("LTE")?"\u4FE1\u566A\u6BD4":p.networkMode.includes("WCDMA")?"\u5BFC\u9891\u4FE1\u53F7\u80FD\u91CF/\u5E72\u6270\u6BD4":"\u4FE1\u566A\u6BD4"})]})}),(0,e.jsx)(f.Z,{xs:6,sm:6,children:(0,e.jsxs)("div",{style:{textAlign:"center"},children:[(0,e.jsx)("div",{style:{fontSize:"24px",fontWeight:"bold",color:p.ecio>=-10?"#52c41a":p.ecio>=-15?"#faad14":"#ff4d4f"},children:p.ecio}),(0,e.jsx)("div",{style:{fontSize:"12px",color:"var(--ant-text-color-secondary)"},children:p.networkMode.includes("NR")||p.networkMode.includes("LTE")?"RSRQ (dB)":p.networkMode.includes("WCDMA")?"ECIO (dB)":"RSSI (dBm)"}),(0,e.jsx)("div",{style:{fontSize:"12px",color:"var(--ant-text-color-secondary)"},children:p.networkMode.includes("NR")||p.networkMode.includes("LTE")?"\u53C2\u8003\u4FE1\u53F7\u63A5\u6536\u8D28\u91CF":p.networkMode.includes("WCDMA")?"\u5BFC\u9891\u4FE1\u9053\u63A5\u6536\u8D28\u91CF":"\u63A5\u6536\u4FE1\u53F7\u5F3A\u5EA6\u6307\u793A"})]})})]})})}),(0,e.jsx)(f.Z,{xs:24,lg:8,children:(0,e.jsx)(T.Z,{size:"small",title:"\u7F51\u7EDC\u53C2\u6570",bordered:!1,style:{background:"#f9f9f9",height:"100%"},children:(0,e.jsxs)(W.Z,{gutter:[16,8],children:[(0,e.jsxs)(f.Z,{span:12,children:[(0,e.jsx)("div",{style:{fontSize:"12px",color:"#666"},children:"PCI:"}),(0,e.jsx)("div",{style:{fontWeight:"bold"},children:p.pci})]}),(0,e.jsxs)(f.Z,{span:12,children:[(0,e.jsx)("div",{style:{fontSize:"12px",color:"#666"},children:"\u9891\u70B9:"}),(0,e.jsx)("div",{style:{fontWeight:"bold"},children:p.channel})]}),(0,e.jsxs)(f.Z,{span:12,children:[(0,e.jsx)("div",{style:{fontSize:"12px",color:"#666"},children:"MCC-MNC:"}),(0,e.jsxs)("div",{style:{fontWeight:"bold"},children:[p.mcc,"-",p.mnc]})]}),(0,e.jsxs)(f.Z,{span:12,children:[(0,e.jsx)("div",{style:{fontSize:"12px",color:"#666"},children:"LAC:"}),(0,e.jsx)("div",{style:{fontWeight:"bold"},children:p.lac})]}),(0,e.jsxs)(f.Z,{span:24,children:[(0,e.jsx)("div",{style:{fontSize:"12px",color:"#666"},children:"\u5C0F\u533AID:"}),(0,e.jsx)("div",{style:{fontWeight:"bold"},children:p.cid})]})]})})}),(0,e.jsx)(f.Z,{xs:24,children:(0,e.jsx)(T.Z,{type:"inner",title:(0,e.jsxs)("span",{children:["\u8F7D\u6CE2\u805A\u5408\u4FE1\u606F",p.carrierCount>0?(0,e.jsxs)("span",{style:{marginLeft:"8px",fontSize:"14px",color:"#1890ff"},children:["(",p.carrierCount,"\u8F7D\u6CE2 | \u603B\u5E26\u5BBD: \u4E0B\u884C",p.dlBandwidth,"MHz/\u4E0A\u884C",p.ulBandwidth,"MHz)"]}):(0,e.jsx)("span",{style:{marginLeft:"8px",fontSize:"14px",color:"var(--ant-text-color-secondary)"},children:"\u65E0\u8F7D\u6CE2"})]}),style:{background:"var(--ant-bg-elevated)",border:"1px solid var(--ant-border-color-split)",boxShadow:"0 1px 2px 0 rgba(0, 0, 0, 0.03), 0 1px 6px -1px rgba(0, 0, 0, 0.02), 0 2px 4px 0 rgba(0, 0, 0, 0.02)"},children:p.carrierInfo.length>0?(0,e.jsx)("div",{children:(0,e.jsx)(W.Z,{gutter:[16,16],children:p.carrierInfo.map(function(c,r){return(0,e.jsx)(f.Z,{xs:24,sm:12,md:8,children:(0,e.jsxs)(T.Z,{size:"small",title:(0,e.jsxs)("span",{style:{color:r===0?"#1890ff":"#666",fontWeight:r===0?"bold":"normal"},children:[r===0?"\u4E3B\u8F7D\u6CE2":"\u8F85\u8F7D\u6CE2 ".concat(r),(0,e.jsxs)("span",{style:{marginLeft:"8px",fontSize:"12px",color:c.sysMode==="NR"?"#52c41a":"#fa8c16"},children:["(",c.sysMode,")"]})]}),style:{borderLeft:r===0?"3px solid #1890ff":c.sysMode==="NR"?"3px solid #52c41a":"3px solid #fa8c16",height:"100%",boxShadow:"0 2px 8px rgba(0,0,0,0.09)"},children:[(0,e.jsxs)("div",{style:{marginBottom:"8px"},children:[(0,e.jsx)("span",{style:{fontWeight:"bold"},children:c.bandShortName}),(0,e.jsxs)("span",{style:{color:"#666",fontSize:"12px",marginLeft:"8px"},children:["(",c.bandDesc,")"]})]}),(0,e.jsxs)(W.Z,{gutter:[8,8],children:[(0,e.jsxs)(f.Z,{span:12,children:[(0,e.jsx)("div",{style:{fontSize:"12px",color:"#666"},children:"\u4E0B\u884C\u9891\u70B9:"}),(0,e.jsx)("div",{children:c.dlFcn})]}),(0,e.jsxs)(f.Z,{span:12,children:[(0,e.jsx)("div",{style:{fontSize:"12px",color:"#666"},children:"\u4E0A\u884C\u9891\u70B9:"}),(0,e.jsx)("div",{children:c.ulFcn})]}),(0,e.jsxs)(f.Z,{span:12,children:[(0,e.jsx)("div",{style:{fontSize:"12px",color:"#666"},children:"\u4E0B\u884C\u9891\u7387:"}),(0,e.jsxs)("div",{children:[c.dlFreq," MHz"]})]}),(0,e.jsxs)(f.Z,{span:12,children:[(0,e.jsx)("div",{style:{fontSize:"12px",color:"#666"},children:"\u4E0A\u884C\u9891\u7387:"}),(0,e.jsxs)("div",{children:[c.ulFreq," MHz"]})]}),(0,e.jsxs)(f.Z,{span:12,children:[(0,e.jsx)("div",{style:{fontSize:"12px",color:"#666"},children:"\u4E0B\u884C\u5E26\u5BBD:"}),(0,e.jsxs)("div",{children:[c.dlBandwidth," MHz"]})]}),(0,e.jsxs)(f.Z,{span:12,children:[(0,e.jsx)("div",{style:{fontSize:"12px",color:"#666"},children:"\u4E0A\u884C\u5E26\u5BBD:"}),(0,e.jsxs)("div",{children:[c.ulBandwidth," MHz"]})]})]})]})},r)})})}):(0,e.jsx)("div",{style:{color:"#666",fontSize:"14px",padding:"16px 0",textAlign:"center"},children:"\u5F53\u524D\u672A\u83B7\u53D6\u5230\u8F7D\u6CE2\u4FE1\u606F\u6216\u672A\u542F\u7528\u8F7D\u6CE2\u805A\u5408"})})})]})})}),(0,e.jsx)(f.Z,{xs:24,md:12,children:(0,e.jsx)(T.Z,{title:(0,e.jsxs)("div",{style:{display:"flex",alignItems:"center",gap:"8px"},children:[(0,e.jsx)("span",{children:"\u7F51\u7EDC\u901F\u7387\u4FE1\u606F"}),(0,e.jsx)("div",{style:{fontSize:"12px",color:"#666",background:"#f5f5f5",padding:"2px 8px",borderRadius:"4px",fontWeight:"normal"},children:"\u5C55\u793A\u7F51\u7EDC\u901F\u7387\u76F8\u5173\u4FE1\u606F"})]}),extra:(0,e.jsx)(An.Z,{checkedChildren:"\u5B9E\u65F6\u7F51\u901F\u5F00\u542F",unCheckedChildren:"\u5B9E\u65F6\u7F51\u901F\u5173\u95ED",checked:fe,onChange:dr}),className:"inner-card",style:{height:"100%"},children:(0,e.jsxs)(W.Z,{gutter:[24,24],children:[(0,e.jsx)(f.Z,{xs:24,children:(0,e.jsxs)(T.Z,{size:"small",title:"\u5B9E\u65F6\u7F51\u901F",bordered:!1,style:{background:"#f9f9f9",position:"relative"},children:[fe?null:(0,e.jsx)("div",{style:{position:"absolute",top:0,left:0,right:0,bottom:0,background:"rgba(255, 255, 255, 0.9)",display:"flex",alignItems:"center",justifyContent:"center",zIndex:1},children:(0,e.jsx)("div",{style:{color:"#999",fontSize:"14px"},children:"\u6682\u672A\u5F00\u542F\u5B9E\u65F6\u7F51\u901F\u76D1\u63A7"})}),(0,e.jsxs)(W.Z,{gutter:[16,16],children:[(0,e.jsx)(f.Z,{xs:12,children:(0,e.jsx)(H.Z,{title:"\u4E0A\u884C\u901F\u7387",value:(B==null?void 0:B.ulPdcpRate)>0?je(B.ulPdcpRate):L?je(L.ulPdcpRate):"0 bps",valueStyle:{color:((B==null?void 0:B.ulPdcpRate)||(L==null?void 0:L.ulPdcpRate)||0)*8>=1e8?"#52c41a":((B==null?void 0:B.ulPdcpRate)||(L==null?void 0:L.ulPdcpRate)||0)*8>=1e7?"#1890ff":"#faad14",fontSize:"18px"}})}),(0,e.jsx)(f.Z,{xs:12,children:(0,e.jsx)(H.Z,{title:"\u4E0B\u884C\u901F\u7387",value:(B==null?void 0:B.dlPdcpRate)>0?je(B.dlPdcpRate):L?je(L.dlPdcpRate):"0 bps",valueStyle:{color:((B==null?void 0:B.dlPdcpRate)||(L==null?void 0:L.dlPdcpRate)||0)*8>=1e8?"#52c41a":((B==null?void 0:B.dlPdcpRate)||(L==null?void 0:L.dlPdcpRate)||0)*8>=1e7?"#1890ff":"#faad14",fontSize:"18px"}})})]})]})}),(0,e.jsx)(f.Z,{xs:24,children:(0,e.jsx)(T.Z,{size:"small",title:"\u5F53\u524D\u7F51\u7EDC",bordered:!1,style:{background:"#f9f9f9"},children:(0,e.jsxs)(W.Z,{gutter:[16,16],children:[(0,e.jsx)(f.Z,{xs:24,sm:12,children:(0,e.jsxs)(W.Z,{gutter:[8,8],children:[(0,e.jsx)(f.Z,{span:12,children:(0,e.jsxs)("div",{style:{background:"#fff",padding:"8px 12px",borderRadius:"4px",height:"100%"},children:[(0,e.jsx)("div",{style:{fontSize:"12px",color:"#666",marginBottom:"4px"},children:"\u4E0A\u884C\u901F\u7387"}),(0,e.jsxs)("div",{style:{fontSize:"16px",fontWeight:500,color:Ae>=50?"#52c41a":Ae>=25?"#faad14":"#ff4d4f"},children:[Ae," ",(0,e.jsx)("span",{style:{fontSize:"12px",color:"#666"},children:"Mbps"})]})]})}),(0,e.jsx)(f.Z,{span:12,children:(0,e.jsxs)("div",{style:{background:"#fff",padding:"8px 12px",borderRadius:"4px",height:"100%"},children:[(0,e.jsx)("div",{style:{fontSize:"12px",color:"#666",marginBottom:"4px"},children:"\u4E0B\u884C\u901F\u7387"}),(0,e.jsxs)("div",{style:{fontSize:"16px",fontWeight:500,color:Pe>=100?"#52c41a":Pe>=50?"#faad14":"#ff4d4f"},children:[Pe," ",(0,e.jsx)("span",{style:{fontSize:"12px",color:"#666"},children:"Mbps"})]})]})})]})}),(0,e.jsx)(f.Z,{xs:24,sm:12,children:(0,e.jsxs)(W.Z,{gutter:[8,8],children:[(0,e.jsx)(f.Z,{span:12,children:(0,e.jsxs)("div",{style:{background:"#fff",padding:"8px 12px",borderRadius:"4px",height:"100%"},children:[(0,e.jsx)("div",{style:{fontSize:"12px",color:"#666",marginBottom:"4px"},children:"\u8FD0\u8425\u5546"}),(0,e.jsx)("div",{style:{fontSize:"14px",fontWeight:500},children:_n})]})}),(0,e.jsx)(f.Z,{span:12,children:(0,e.jsxs)("div",{style:{background:"#fff",padding:"8px 12px",borderRadius:"4px",height:"100%"},children:[(0,e.jsx)("div",{style:{fontSize:"12px",color:"#666",marginBottom:"4px"},children:"APN"}),(0,e.jsx)("div",{style:{fontSize:"14px",fontWeight:500},children:mr||"\u672A\u77E5"})]})})]})}),(0,e.jsx)(f.Z,{xs:24,children:(0,e.jsxs)("div",{style:{background:"#fff",padding:"8px 12px",borderRadius:"4px"},children:[(0,e.jsx)("div",{style:{fontSize:"12px",color:"#666",marginBottom:"4px"},children:"QCI (\u670D\u52A1\u8D28\u91CF\u7B49\u7EA7)"}),(0,e.jsxs)("div",{style:{fontSize:"14px",color:"#666"},children:[xn.split("\uFF1A")[0],(0,e.jsx)("span",{style:{marginLeft:"8px",fontSize:"12px",color:"#999"},children:xn.split("\uFF1A")[1]})]})]})})]})})})]})})}),(0,e.jsx)(f.Z,{xs:24,md:12,children:(0,e.jsx)(T.Z,{title:(0,e.jsxs)("div",{style:{display:"flex",alignItems:"center",gap:"8px"},children:[(0,e.jsx)("span",{children:"\u6D41\u91CF\u7EDF\u8BA1"}),(0,e.jsx)("div",{style:{fontSize:"12px",color:"#666",background:"#f5f5f5",padding:"2px 8px",borderRadius:"4px",fontWeight:"normal"},children:"\u5C55\u793A\u7F51\u7EDC\u8FDE\u63A5\u65F6\u95F4\u548C\u6D41\u91CF\u4FE1\u606F"})]}),extra:(0,e.jsxs)(ce.ZP,{type:"link",size:"small",style:{padding:"0 8px",height:"28px",display:"flex",alignItems:"center",gap:"4px",background:N.flowStats.enabled?"#e6f7ff":"transparent",border:"1px solid #91d5ff",borderRadius:"4px"},onClick:function(r){r.target.closest(".ant-input-number")||te("flowStats",!N.flowStats.enabled,N.flowStats.interval)},children:[(0,e.jsx)("span",{children:"\u81EA\u52A8\u5237\u65B0"}),N.flowStats.enabled&&(0,e.jsx)(be.Z,{min:1,max:60,value:N.flowStats.interval,onChange:function(r){return te("flowStats",!0,r||5)},style:{width:45},size:"small",bordered:!1}),N.flowStats.enabled&&(0,e.jsx)("span",{children:"\u79D2"})]}),className:"inner-card",style:{height:"100%"},children:(0,e.jsxs)(W.Z,{gutter:[24,24],children:[(0,e.jsx)(f.Z,{xs:24,children:(0,e.jsx)(T.Z,{size:"small",title:"\u6700\u540E\u4E00\u6B21\u8FDE\u63A5",bordered:!1,style:{background:"#f9f9f9"},children:(0,e.jsxs)(W.Z,{gutter:[24,16],children:[(0,e.jsx)(f.Z,{xs:24,sm:8,children:(0,e.jsx)(H.Z,{title:"\u8FDE\u63A5\u65F6\u957F",value:bn(se.lastDsTime),valueStyle:{fontSize:"16px"}})}),(0,e.jsx)(f.Z,{xs:24,sm:8,children:(0,e.jsx)(H.Z,{title:"\u4E0A\u4F20\u6D41\u91CF",value:Fe(se.lastTxFlow),valueStyle:{fontSize:"16px"}})}),(0,e.jsx)(f.Z,{xs:24,sm:8,children:(0,e.jsx)(H.Z,{title:"\u4E0B\u8F7D\u6D41\u91CF",value:Fe(se.lastRxFlow),valueStyle:{fontSize:"16px"}})})]})})}),(0,e.jsx)(f.Z,{xs:24,children:(0,e.jsx)(T.Z,{size:"small",title:"\u7D2F\u8BA1\u7EDF\u8BA1",bordered:!1,style:{background:"#f9f9f9"},children:(0,e.jsxs)(W.Z,{gutter:[24,16],children:[(0,e.jsx)(f.Z,{xs:24,sm:8,children:(0,e.jsx)(H.Z,{title:"\u603B\u8FDE\u63A5\u65F6\u957F",value:bn(se.totalDsTime),valueStyle:{fontSize:"16px"}})}),(0,e.jsx)(f.Z,{xs:24,sm:8,children:(0,e.jsx)(H.Z,{title:"\u603B\u4E0A\u4F20\u6D41\u91CF",value:Fe(se.totalTxFlow),valueStyle:{fontSize:"16px"}})}),(0,e.jsx)(f.Z,{xs:24,sm:8,children:(0,e.jsx)(H.Z,{title:"\u603B\u4E0B\u8F7D\u6D41\u91CF",value:Fe(se.totalRxFlow),valueStyle:{fontSize:"16px"}})})]})})}),(0,e.jsx)(f.Z,{xs:24,style:{marginTop:8,textAlign:"right"},children:(0,e.jsxs)(Bn.Z,{children:[(0,e.jsx)(ce.ZP,{type:we?"primary":"default",onClick:function(){return or(!we)},size:"middle",children:we?"\u663E\u793A\u65F6\u5206\u79D2":"\u663E\u793A\u5929\u6570"}),(0,e.jsx)(ce.ZP,{danger:!0,onClick:Dr,size:"middle",children:"\u6E05\u96F6"})]})})]})})}),(0,e.jsx)(f.Z,{xs:24,md:12,children:cr()}),(0,e.jsx)(f.Z,{xs:24,children:(0,e.jsx)(T.Z,{title:(0,e.jsxs)("div",{style:{display:"flex",alignItems:"center",gap:"8px"},children:[(0,e.jsx)("span",{children:"\u6A21\u7EC4\u6E29\u5EA6\u76D1\u63A7"}),(0,e.jsx)("div",{style:{fontSize:"12px",color:"#666",background:"#f5f5f5",padding:"2px 8px",borderRadius:"4px",fontWeight:"normal"},children:"5G\u6A21\u7EC4\u5404\u529F\u80FD\u6A21\u5757\u6E29\u5EA6\u72B6\u6001"})]}),extra:(0,e.jsxs)(ce.ZP,{type:"link",size:"small",style:{padding:"0 8px",height:"28px",display:"flex",alignItems:"center",gap:"4px",background:N.tempMonitor.enabled?"#e6f7ff":"transparent",border:"1px solid #91d5ff",borderRadius:"4px"},onClick:function(r){r.target.closest(".ant-input-number")||te("tempMonitor",!N.tempMonitor.enabled,N.tempMonitor.interval)},children:[(0,e.jsx)("span",{children:"\u81EA\u52A8\u5237\u65B0"}),N.tempMonitor.enabled&&(0,e.jsx)(be.Z,{min:1,max:60,value:N.tempMonitor.interval,onChange:function(r){return te("tempMonitor",!0,r||5)},style:{width:45},size:"small",bordered:!1}),N.tempMonitor.enabled&&(0,e.jsx)("span",{children:"\u79D2"})]}),className:"inner-card",bodyStyle:{padding:"24px"},children:(0,e.jsxs)(W.Z,{gutter:[16,16],children:[(0,e.jsx)(f.Z,{xs:24,sm:12,md:8,lg:6,children:(0,e.jsx)(T.Z,{size:"small",bordered:!1,className:"temperature-card",style:{background:"#ffffff",boxShadow:"0 2px 8px rgba(0,0,0,0.08)",border:"1px solid #f0f0f0",transition:"all 0.3s ease"},hoverable:!0,children:(0,e.jsx)(H.Z,{title:(0,e.jsxs)("span",{children:["3G PA\u6E29\u5EA6"," ",(0,e.jsx)("span",{style:{fontSize:"12px",color:"#666"},children:"(\u529F\u653E\u6E29\u5EA6)"})]}),value:P.sub3GPA,suffix:"\xB0C",valueStyle:{fontSize:"24px",fontWeight:500,color:P.sub3GPA<=45?"#52c41a":P.sub3GPA<=65?"#faad14":"#ff4d4f"}})})}),(0,e.jsx)(f.Z,{xs:24,sm:12,md:8,lg:6,children:(0,e.jsx)(T.Z,{size:"small",bordered:!1,className:"temperature-card",style:{background:"#ffffff",boxShadow:"0 2px 8px rgba(0,0,0,0.08)",border:"1px solid #f0f0f0",transition:"all 0.3s ease"},hoverable:!0,children:(0,e.jsx)(H.Z,{title:(0,e.jsxs)("span",{children:["6G PA\u6E29\u5EA6"," ",(0,e.jsx)("span",{style:{fontSize:"12px",color:"#666"},children:"(\u529F\u653E\u6E29\u5EA6)"})]}),value:P.sub6GPA,suffix:"\xB0C",valueStyle:{fontSize:"24px",fontWeight:500,color:P.sub6GPA<=45?"#52c41a":P.sub6GPA<=65?"#faad14":"#ff4d4f"}})})}),(0,e.jsx)(f.Z,{xs:24,sm:12,md:8,lg:6,children:(0,e.jsx)(T.Z,{size:"small",bordered:!1,className:"temperature-card",style:{background:"#ffffff",boxShadow:"0 2px 8px rgba(0,0,0,0.08)",border:"1px solid #f0f0f0",transition:"all 0.3s ease"},hoverable:!0,children:(0,e.jsx)(H.Z,{title:(0,e.jsxs)("span",{children:["MIMO PA\u6E29\u5EA6"," ",(0,e.jsx)("span",{style:{fontSize:"12px",color:"#666"},children:"(\u591A\u5165\u591A\u51FA\u529F\u653E)"})]}),value:P.mimoPa,suffix:"\xB0C",valueStyle:{fontSize:"24px",fontWeight:500,color:P.mimoPa<=45?"#52c41a":P.mimoPa<=65?"#faad14":"#ff4d4f"}})})}),(0,e.jsx)(f.Z,{xs:24,sm:12,md:8,lg:6,children:(0,e.jsx)(T.Z,{size:"small",bordered:!1,className:"temperature-card",style:{background:"#ffffff",boxShadow:"0 2px 8px rgba(0,0,0,0.08)",border:"1px solid #f0f0f0",transition:"all 0.3s ease"},hoverable:!0,children:(0,e.jsx)(H.Z,{title:(0,e.jsxs)("span",{children:["TCXO\u6E29\u5EA6 ",(0,e.jsx)("span",{style:{fontSize:"12px",color:"#666"},children:"(\u6676\u632F\u6E29\u5EA6)"})]}),value:P.tcxo,suffix:"\xB0C",valueStyle:{fontSize:"24px",fontWeight:500,color:P.tcxo<=45?"#52c41a":P.tcxo<=65?"#faad14":"#ff4d4f"}})})}),(0,e.jsx)(f.Z,{xs:24,sm:12,md:8,lg:6,children:(0,e.jsx)(T.Z,{size:"small",bordered:!1,className:"temperature-card",style:{background:"#ffffff",boxShadow:"0 2px 8px rgba(0,0,0,0.08)",border:"1px solid #f0f0f0",transition:"all 0.3s ease"},hoverable:!0,children:(0,e.jsx)(H.Z,{title:(0,e.jsxs)("span",{children:["AP1\u6E29\u5EA6"," ",(0,e.jsx)("span",{style:{fontSize:"12px",color:"#666"},children:"(\u5E94\u7528\u5904\u7406\u56681)"})]}),value:P.ap1,suffix:"\xB0C",valueStyle:{fontSize:"24px",fontWeight:500,color:P.ap1<=45?"#52c41a":P.ap1<=65?"#faad14":"#ff4d4f"}})})}),(0,e.jsx)(f.Z,{xs:24,sm:12,md:8,lg:6,children:(0,e.jsx)(T.Z,{size:"small",bordered:!1,className:"temperature-card",style:{background:"#ffffff",boxShadow:"0 2px 8px rgba(0,0,0,0.08)",border:"1px solid #f0f0f0",transition:"all 0.3s ease"},hoverable:!0,children:(0,e.jsx)(H.Z,{title:(0,e.jsxs)("span",{children:["AP2\u6E29\u5EA6"," ",(0,e.jsx)("span",{style:{fontSize:"12px",color:"#666"},children:"(\u5E94\u7528\u5904\u7406\u56682)"})]}),value:P.ap2,suffix:"\xB0C",valueStyle:{fontSize:"24px",fontWeight:500,color:P.ap2<=45?"#52c41a":P.ap2<=65?"#faad14":"#ff4d4f"}})})}),(0,e.jsx)(f.Z,{xs:24,sm:12,md:8,lg:6,children:(0,e.jsx)(T.Z,{size:"small",bordered:!1,className:"temperature-card",style:{background:"#ffffff",boxShadow:"0 2px 8px rgba(0,0,0,0.08)",border:"1px solid #f0f0f0",transition:"all 0.3s ease"},hoverable:!0,children:(0,e.jsx)(H.Z,{title:(0,e.jsxs)("span",{children:["Modem1\u6E29\u5EA6"," ",(0,e.jsx)("span",{style:{fontSize:"12px",color:"#666"},children:"(\u8C03\u5236\u89E3\u8C03\u56681)"})]}),value:P.modem1,suffix:"\xB0C",valueStyle:{fontSize:"24px",fontWeight:500,color:P.modem1<=45?"#52c41a":P.modem1<=65?"#faad14":"#ff4d4f"}})})})]})})})]}),(0,e.jsx)("style",{dangerouslySetInnerHTML:{__html:`
        .network-info-card .ant-card-head-title {
          white-space: normal;
          overflow: visible;
        }
        
        .network-info-card .ant-card-extra {
          margin-left: 10px;
          white-space: normal;
        }
        
        @media (max-width: 576px) {
          .network-info-card .ant-card-extra {
            margin-left: 0;
            margin-top: 5px;
          }
          
          .inner-card .ant-card-head {
            min-height: unset;
            padding: 0 12px;
          }
          
          .inner-card .ant-card-head-title,
          .inner-card .ant-card-extra {
            padding: 8px 0;
            font-size: 14px;
          }
          
          .inner-card .ant-card-body {
            padding: 12px;
          }
          
          .ant-statistic-title {
            font-size: 12px;
          }
          
          .ant-statistic-content {
            font-size: 16px;
          }
        }
        
        .stats-card {
          background: var(--ant-card-bg);
          border-radius: 8px;
          transition: all 0.3s;
        }
        
        .stats-card:hover {
          box-shadow: 0 2px 8px var(--ant-shadow-1);
        }
        
        .stats-card .ant-card-head {
          min-height: 40px;
          padding: 0 16px;
          border-bottom: 1px solid var(--ant-border-color-split);
        }
        
        .stats-card .ant-card-head-title {
          padding: 12px 0;
          font-size: 16px;
          font-weight: 500;
        }
        
        .stats-card .ant-card-body {
          padding: 16px;
        }
        
        .stats-card .ant-statistic-title {
          margin-bottom: 8px;
          color: var(--ant-text-color-secondary);
        }
        
        .stats-card .ant-statistic-content {
          font-weight: 500;
          color: var(--ant-text-color);
        }
        
        @media (max-width: 576px) {
          .stats-card .ant-card-body {
            padding: 12px;
          }
          
          .stats-card .ant-statistic-content {
            font-size: 16px !important;
          }
        }
        
        .speed-info-card {
          background: var(--ant-card-bg);
          border-radius: 8px;
          transition: all 0.3s;
          height: 100%;
          border: 1px solid var(--ant-border-color-split);
        }
        
        .speed-info-card:hover {
          box-shadow: 0 4px 12px var(--ant-shadow-2);
          transform: translateY(-2px);
          border-color: var(--ant-primary-color);
        }
        
        .speed-info-card .ant-statistic-title {
          margin-bottom: 12px;
          color: var(--ant-text-color-secondary);
        }
        
        .speed-info-card .ant-statistic-content {
          line-height: 1.4;
          white-space: normal;
          word-break: break-all;
          color: var(--ant-text-color);
        }
        
        .speed-info-card .ant-statistic-content-suffix {
          color: var(--ant-text-color-secondary);
          font-size: 14px;
        }
        
        @media (max-width: 576px) {
          .speed-info-card {
            margin-bottom: 12px;
          }
          
          .speed-info-card .ant-statistic-content {
            font-size: 16px !important;
          }
        }
        
        .ant-input-number-handler-wrap {
          opacity: 0.5;
        }
        
        .ant-input-number:hover .ant-input-number-handler-wrap {
          opacity: 1;
        }
        
    
        
        .ant-input-number {
          background: transparent;
        }
        
        .ant-input-number-input {
          text-align: center;
          color: var(--ant-primary-color);
        }
        
        .ant-btn-text {
          color: var(--ant-text-color-secondary);
        }
        
        .ant-btn-text:hover {
          color: var(--ant-primary-color);
          background: transparent;
        }
        
        .ant-btn-link {
          color: var(--ant-primary-color);
        }
        
        .ant-btn-link:hover {
          color: var(--ant-primary-color-hover);
          background: var(--ant-primary-1);
          border-color: var(--ant-primary-color-hover);
        }
        
        .temperature-card {
          background: #ffffff;
          border-radius: 8px;
          transition: all 0.3s ease;
        }
        
        .temperature-card:hover {
          transform: translateY(-2px);
          box-shadow: 0 4px 12px rgba(0,0,0,0.12);
          border-color: #1890ff;
        }
        
        .temperature-card .ant-statistic-title {
          margin-bottom: 8px;
          color: #666;
        }
        
        .temperature-card .ant-statistic-content {
          display: flex;
          align-items: center;
          justify-content: center;
        }
        
        .temperature-card .ant-statistic-content-suffix {
          margin-left: 4px;
          font-size: 16px;
          color: #666;
        }
        
        @media (max-width: 576px) {
          .temperature-card {
            margin-bottom: 12px;
          }
          
          .temperature-card .ant-statistic-content {
            font-size: 20px !important;
          }
        }
        
        @media (max-width: 576px) {
          .ant-col-xs-12 {
            margin-bottom: 8px;
          }
          
          .ant-col-xs-12 .ant-statistic-content,
          .ant-col-xs-12 div[style*="fontSize: '22px'"] {
            font-size: 20px !important;
          }
          
          .ant-col-xs-12 div[style*="fontSize: '12px'"] {
            font-size: 11px !important;
            line-height: 1.2;
          }
        }
      `}}),(0,e.jsx)("style",{jsx:!0,global:!0,children:`
        @media screen and (max-width: 576px) {
          .signal-board-card {
            margin-bottom: 16px;
          }

          .signal-indicator {
            padding: 12px !important;
          }

          .signal-value {
            font-size: 20px !important;
          }

          .signal-unit {
            font-size: 12px !important;
          }

          .signal-title {
            font-size: 13px !important;
          }

          .signal-desc {
            font-size: 11px !important;
          }
        }
      `}),(0,e.jsx)("style",{jsx:!0,global:!0,children:`
        .network-dashboard {
          overflow: hidden;
          background: #fff;
          border-radius: 16px;
          box-shadow: 0 4px 20px rgba(0, 0, 0, 0.08);
        }

        .dashboard-header {
          display: flex;
          align-items: center;
          justify-content: space-between;
          padding: 16px 24px;
          color: white;
          background: linear-gradient(135deg, #1a237e, #0d47a1);
        }

        .network-status {
          display: flex;
          gap: 12px;
          align-items: center;
        }

        .status-badge {
          padding: 4px 12px;
          font-weight: 600;
          font-size: 14px;
          background: rgba(255, 255, 255, 0.2);
          border-radius: 20px;
        }

        .status-badge[data-mode='NR'] {
          background: #00c853;
        }

        .status-badge[data-mode='LTE'] {
          background: #2962ff;
        }

        .signal-overview {
          padding: 24px;
          background: linear-gradient(to bottom, #f5f5f5, #fff);
        }

        .signal-strength {
          display: flex;
          gap: 20px;
          align-items: center;
        }

        .signal-icon {
          color: #1a237e;
          font-size: 48px;
        }

        .signal-value {
          color: #1a237e;
          font-weight: 700;
          font-size: 36px;
        }

        .signal-metrics {
          display: flex;
          gap: 16px;
          color: #666;
          font-size: 14px;
        }

        .metrics-grid {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
          gap: 16px;
          padding: 24px;
        }

        .metric-card {
          padding: 20px;
          background: #f8f9fa;
          border-radius: 12px;
          transition: all 0.3s ease;
        }

        .metric-card:hover {
          box-shadow: 0 4px 12px rgba(0, 0, 0, 0.05);
          transform: translateY(-2px);
        }

        .metric-header {
          display: flex;
          gap: 8px;
          align-items: center;
          margin-bottom: 12px;
        }

        .metric-icon {
          font-size: 20px;
        }

        .metric-title {
          color: #1a237e;
          font-weight: 600;
        }

        .metric-value {
          margin-bottom: 8px;
          color: #1a237e;
          font-weight: 700;
          font-size: 28px;
        }

        .metric-unit {
          margin-left: 4px;
          color: #666;
          font-size: 14px;
        }

        .metric-desc {
          color: #666;
          font-size: 13px;
        }

        .carrier-info {
          padding: 24px;
          background: #f8f9fa;
        }

        .carrier-header {
          display: flex;
          align-items: center;
          justify-content: space-between;
          margin-bottom: 16px;
          color: #1a237e;
          font-weight: 600;
        }

        .carrier-count {
          padding: 4px 12px;
          font-size: 14px;
          background: #e3f2fd;
          border-radius: 20px;
        }

        .carrier-grid {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
          gap: 16px;
        }

        .carrier-card {
          padding: 16px;
          background: white;
          border: 1px solid #e0e0e0;
          border-radius: 12px;
          transition: all 0.3s ease;
        }

        .carrier-card[data-primary='true'] {
          background: linear-gradient(135deg, #e8eaf6, #fff);
          border-color: #1a237e;
        }

        .carrier-title {
          display: flex;
          justify-content: space-between;
          margin-bottom: 12px;
          color: #1a237e;
          font-weight: 600;
        }

        .carrier-type {
          padding: 2px 8px;
          font-size: 12px;
          background: #e3f2fd;
          border-radius: 12px;
        }

        .carrier-details {
          display: grid;
          gap: 8px;
        }

        .detail-item {
          display: flex;
          align-items: center;
          justify-content: space-between;
          font-size: 14px;
        }

        .detail-item span {
          color: #666;
        }

        .detail-item strong {
          color: #1a237e;
        }

        @media (max-width: 576px) {
          .dashboard-header {
            padding: 12px 16px;
          }

          .signal-overview {
            padding: 16px;
          }

          .signal-icon {
            font-size: 36px;
          }

          .signal-value {
            font-size: 28px;
          }

          .metrics-grid {
            grid-template-columns: 1fr;
            padding: 16px;
          }

          .metric-card {
            padding: 16px;
          }

          .carrier-info {
            padding: 16px;
          }

          .carrier-grid {
            grid-template-columns: 1fr;
          }
        }
      `}),(0,e.jsx)("style",{jsx:!0,children:`
        .network-params {
          display: flex;
          flex-direction: column;
          gap: 12px;
        }

        .param-item {
          padding: 12px;
          background: var(--ant-card-bg);
          border: 1px solid var(--ant-border-color-split);
          border-radius: 8px;
          transition: all 0.3s ease;
        }

        .param-item:hover {
          border-color: var(--ant-primary-color);
          box-shadow: 0 2px 8px var(--ant-shadow-1);
          transform: translateY(-1px);
        }

        .param-header {
          display: flex;
          align-items: center;
          justify-content: space-between;
          margin-bottom: 8px;
        }

        .param-label {
          color: var(--ant-text-color-secondary);
          font-weight: 500;
          font-size: 13px;
        }

        .param-icon {
          width: 20px;
          height: 20px;
          background-repeat: no-repeat;
          background-position: center;
          background-size: contain;
          opacity: 0.5;
        }

        .param-content {
          display: flex;
          gap: 8px;
          align-items: center;
          font-family: 'Roboto Mono', monospace;
        }

        .primary-value {
          color: var(--ant-primary-color);
          font-weight: 600;
          font-size: 15px;
        }

        .divider {
          color: var(--ant-border-color-split);
          font-size: 15px;
        }

        .secondary-value {
          color: var(--ant-text-color);
          font-weight: 600;
          font-size: 15px;
        }

        .signal-metrics-grid {
          display: grid;
          grid-template-columns: repeat(2, 1fr);
          gap: 12px;
        }

        .signal-group {
          flex: 1;
          padding: 12px;
          background: var(--ant-card-bg);
          border: 1px solid var(--ant-border-color-split);
          border-radius: 8px;
          transition: all 0.3s ease;
        }

        .signal-group:hover {
          border-color: var(--ant-primary-color);
          box-shadow: 0 2px 8px var(--ant-shadow-1);
          transform: translateY(-2px);
        }

        .param-values {
          display: flex;
          gap: 8px;
          align-items: center;
          margin-bottom: 4px;
          color: var(--ant-text-color);
          font-weight: 600;
          font-size: 15px;
        }

        .param-desc {
          color: var(--ant-text-color-secondary);
          font-size: 12px;
        }

        @media (max-width: 576px) {
          .network-params {
            gap: 8px;
          }

          .param-item {
            padding: 10px;
          }

          .param-label {
            font-size: 12px;
          }

          .primary-value,
          .secondary-value,
          .divider {
            font-size: 13px;
          }

          .signal-metrics-grid {
            grid-template-columns: 1fr;
            gap: 8px;
          }

          .signal-group {
            padding: 10px;
          }

          .param-values {
            font-size: 14px;
          }

          .param-desc {
            font-size: 11px;
          }
        }
      `}),(0,e.jsx)("style",{jsx:!0,children:`
        .signal-metrics-grid {
          display: grid;
          grid-template-columns: repeat(2, 1fr);
          gap: 12px;
        }

        .signal-group {
          flex: 1;
          padding: 12px;
          background: white;
          border: 1px solid #f0f0f0;
          border-radius: 8px;
          transition: all 0.3s ease;
        }

        .signal-group:hover {
          border-color: #1890ff;
          box-shadow: 0 2px 8px rgba(0, 0, 0, 0.05);
          transform: translateY(-2px);
        }

        .param-label {
          display: flex;
          gap: 6px;
          align-items: center;
          margin-bottom: 4px;
          color: #666;
          font-size: 13px;
        }

        .param-icon {
          font-size: 14px;
        }

        .param-values {
          display: flex;
          gap: 8px;
          align-items: center;
          margin-bottom: 4px;
          color: #1a237e;
          font-weight: 600;
          font-size: 15px;
        }

        .param-desc {
          color: #8c8c8c;
          font-size: 12px;
        }

        @media (max-width: 576px) {
          .signal-metrics-grid {
            grid-template-columns: 1fr;
            gap: 8px;
          }

          .signal-group {
            padding: 10px;
          }

          .param-values {
            font-size: 14px;
          }

          .param-desc {
            font-size: 11px;
          }
        }
      `}),(0,e.jsx)("style",{jsx:!0,children:`
        .signal-dashboard {
          padding: 4px 0;
        }

        .signal-strength-section {
          display: flex;
          gap: 16px;
          align-items: center;
        }

        .signal-metrics {
          display: flex;
          flex: 1;
          gap: 16px;
          align-items: center;
        }

        .signal-icon-wrapper {
          display: flex;
          flex-direction: column;
          gap: 4px;
          align-items: center;
          width: 60px;
          padding-right: 12px;
          border-right: 1px solid #f0f0f0;
        }

        .signal-percent {
          color: #262626;
          font-weight: 600;
          font-size: 14px;
        }

        .metrics-container {
          display: flex;
          flex: 1;
          gap: 12px;
          align-items: center;
          padding-left: 12px;
        }

        .metric-item {
          display: flex;
          flex: 1;
          flex-direction: column;
          gap: 2px;
          align-items: center;
          padding: 4px;
          text-align: center;
          border-radius: 4px;
          transition: all 0.3s ease;
        }

        .metric-item:hover {
          background: rgba(0, 0, 0, 0.02);
        }

        .metric-label {
          color: #8c8c8c;
          font-size: 12px;
        }

        .metric-value {
          font-weight: 600;
          font-size: 16px;
        }

        .metric-desc {
          max-width: 100px;
          color: #8c8c8c;
          font-size: 11px;
        }

        .carrier-list {
          display: flex;
          flex-direction: column;
          gap: 16px;
          max-width: 280px;
          margin-top: 16px;
          padding: 16px;
        }

        .carrier-item {
          display: flex;
          flex-direction: column;
          padding: 12px;
          background: #fff;
          border-radius: 8px;
          box-shadow: 0 1px 2px rgba(0, 0, 0, 0.05);
        }

        .carrier-header {
          display: flex;
          gap: 8px;
          align-items: center;
          margin-bottom: 12px;
          padding-bottom: 8px;
          border-bottom: 1px solid #f0f0f0;
        }

        .band-name {
          color: #1f1f1f;
          font-weight: 600;
          font-size: 14px;
        }

        .band-desc {
          margin-left: 4px;
          color: #666;
          font-size: 12px;
        }

        .freq-info {
          display: flex;
          flex-direction: column;
          gap: 12px;
        }

        .freq-group {
          display: flex;
          flex-direction: column;
          gap: 4px;
        }

        .freq-title {
          margin-bottom: 4px;
          color: #666;
          font-size: 13px;
        }

        .freq-row {
          display: grid;
          grid-template-columns: repeat(3, 1fr);
          gap: 8px;
          align-items: center;
          padding: 6px 8px;
          background: #f9f9f9;
          border-radius: 4px;
        }

        .freq-value {
          color: #1f1f1f;
          font-size: 13px;
          font-family: 'SF Mono', SFMono-Regular, Consolas, monospace;
          text-align: center;
        }

        .freq-value:first-child {
          text-align: left;
        }

        .freq-value:last-child {
          text-align: right;
        }

        @media (max-width: 768px) {
          .carrier-list {
            max-width: none;
          }

          .carrier-item {
            padding: 12px;
          }
        }
      `}),(0,e.jsx)("style",{jsx:!0,children:`
        .carrier-info {
          margin-top: 16px;
        }

        .carrier-list {
          display: flex;
          flex-direction: column;
          gap: 8px;
          max-width: 280px;
        }

        .carrier-item {
          padding: 12px;
          background: var(--ant-card-bg);
          border-radius: 6px;
          box-shadow: 0 1px 2px var(--ant-shadow-1);
        }

        .carrier-header {
          display: flex;
          gap: 8px;
          align-items: center;
          margin-bottom: 8px;
        }

        .band-name {
          color: var(--ant-text-color);
          font-weight: 600;
          font-size: 12px;
        }

        .band-desc {
          color: var(--ant-text-color-secondary);
          font-size: 11px;
        }

        .freq-info {
          display: flex;
          flex-direction: column;
          gap: 4px;
        }

        .freq-row {
          display: flex;
          gap: 8px;
          align-items: center;
          padding: 4px 8px;
          background: var(--ant-bg-elevated);
          border-radius: 4px;
        }

        .freq-label {
          flex-shrink: 0;
          width: 32px;
          color: var(--ant-text-color-secondary);
          font-size: 11px;
        }

        .freq-value {
          color: var(--ant-text-color);
          font-size: 11px;
          font-family: 'SF Mono', SFMono-Regular, Consolas, monospace;
        }

        .temperature-card {
          background: #ffffff;
          border-radius: 8px;
          transition: all 0.3s ease;
        }

        .temperature-card:hover {
          transform: translateY(-2px);
          box-shadow: 0 4px 12px rgba(0,0,0,0.12);
          border-color: #1890ff;
        }

        .temperature-value {
          color: var(--ant-text-color);
          font-size: 24px;
          font-weight: 500;
        }

        .temperature-value.normal {
          color: var(--ant-success-color);
        }

        .temperature-value.warning {
          color: var(--ant-warning-color);
        }

        .temperature-value.danger {
          color: var(--ant-error-color);
        }

        .temperature-label {
          color: var(--ant-text-color-secondary);
          font-size: 12px;
        }

        @media (max-width: 768px) {
          .carrier-list {
            max-width: none;
          }

          .carrier-item {
            padding: 12px;
          }

          .temperature-card {
            margin-bottom: 12px;
          }
        }
      `})]})};Ze.default=Zn}}]);
