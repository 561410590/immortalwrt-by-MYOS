'use strict';
'require baseclass';
'require dom';
'require network';
'require rpc';
'require uci';
'require ui';

var callFrequencies = rpc.declare({
	object: 'iwinfo', method: 'freqlist', params: [ 'device' ], expect: { results: [] }
});
var callPowers = rpc.declare({
	object: 'iwinfo', method: 'txpowerlist', params: [ 'device' ], expect: { results: [] }
});
var nextId = 0;
var callWifiConfig = rpc.declare({ object: 'uci', method: 'get', params: [ 'config' ], expect: { values: {} } });
var closeWifiQr = null;
/* QRCode.js, MIT License. Copyright (c) 2012 davidshimjs.
 * QRCode for JavaScript, Copyright (c) 2009 Kazuhiko Arase.
 * https://github.com/davidshimjs/qrcodejs
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to
 * do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * Embedded encoder only; UTF-8 uses TextEncoder; rendering is scoped SVG.
 */
var wifiQrMatrix = (function() {
function t(t){this.mode=r.MODE_8BIT_BYTE;this.data=t;this.parsedData=Array.from(new TextEncoder().encode(t));}function e(t,e){this.typeNumber=t,this.errorCorrectLevel=e,this.modules=null,this.moduleCount=0,this.dataCache=null,this.dataList=[]}t.prototype={getLength:function(t){return this.parsedData.length},write:function(t){for(var e=0,r=this.parsedData.length;e<r;e++)t.put(this.parsedData[e],8)}},e.prototype={addData:function(e){var r=new t(e);this.dataList.push(r),this.dataCache=null},isDark:function(t,e){if(t<0||this.moduleCount<=t||e<0||this.moduleCount<=e)throw new Error(t+","+e);return this.modules[t][e]},getModuleCount:function(){return this.moduleCount},make:function(){this.makeImpl(!1,this.getBestMaskPattern())},makeImpl:function(t,r){this.moduleCount=4*this.typeNumber+17,this.modules=new Array(this.moduleCount);for(var o=0;o<this.moduleCount;o++){this.modules[o]=new Array(this.moduleCount);for(var i=0;i<this.moduleCount;i++)this.modules[o][i]=null}this.setupPositionProbePattern(0,0),this.setupPositionProbePattern(this.moduleCount-7,0),this.setupPositionProbePattern(0,this.moduleCount-7),this.setupPositionAdjustPattern(),this.setupTimingPattern(),this.setupTypeInfo(t,r),this.typeNumber>=7&&this.setupTypeNumber(t),null==this.dataCache&&(this.dataCache=e.createData(this.typeNumber,this.errorCorrectLevel,this.dataList)),this.mapData(this.dataCache,r)},setupPositionProbePattern:function(t,e){for(var r=-1;r<=7;r++)if(!(t+r<=-1||this.moduleCount<=t+r))for(var o=-1;o<=7;o++)e+o<=-1||this.moduleCount<=e+o||(this.modules[t+r][e+o]=0<=r&&r<=6&&(0==o||6==o)||0<=o&&o<=6&&(0==r||6==r)||2<=r&&r<=4&&2<=o&&o<=4)},getBestMaskPattern:function(){for(var t=0,e=0,r=0;r<8;r++){this.makeImpl(!0,r);var o=g.getLostPoint(this);(0==r||t>o)&&(t=o,e=r)}return e},createMovieClip:function(t,e,r){var o=t.createEmptyMovieClip(e,r);this.make();for(var i=0;i<this.modules.length;i++)for(var n=1*i,a=0;a<this.modules[i].length;a++){var s=1*a;this.modules[i][a]&&(o.beginFill(0,100),o.moveTo(s,n),o.lineTo(s+1,n),o.lineTo(s+1,n+1),o.lineTo(s,n+1),o.endFill())}return o},setupTimingPattern:function(){for(var t=8;t<this.moduleCount-8;t++)null==this.modules[t][6]&&(this.modules[t][6]=t%2==0);for(var e=8;e<this.moduleCount-8;e++)null==this.modules[6][e]&&(this.modules[6][e]=e%2==0)},setupPositionAdjustPattern:function(){for(var t=g.getPatternPosition(this.typeNumber),e=0;e<t.length;e++)for(var r=0;r<t.length;r++){var o=t[e],i=t[r];if(null==this.modules[o][i])for(var n=-2;n<=2;n++)for(var a=-2;a<=2;a++)this.modules[o+n][i+a]=-2==n||2==n||-2==a||2==a||0==n&&0==a}},setupTypeNumber:function(t){for(var e=g.getBCHTypeNumber(this.typeNumber),r=0;r<18;r++){var o=!t&&1==(e>>r&1);this.modules[Math.floor(r/3)][r%3+this.moduleCount-8-3]=o}for(r=0;r<18;r++){o=!t&&1==(e>>r&1);this.modules[r%3+this.moduleCount-8-3][Math.floor(r/3)]=o}},setupTypeInfo:function(t,e){for(var r=this.errorCorrectLevel<<3|e,o=g.getBCHTypeInfo(r),i=0;i<15;i++){var n=!t&&1==(o>>i&1);i<6?this.modules[i][8]=n:i<8?this.modules[i+1][8]=n:this.modules[this.moduleCount-15+i][8]=n}for(i=0;i<15;i++){n=!t&&1==(o>>i&1);i<8?this.modules[8][this.moduleCount-i-1]=n:i<9?this.modules[8][15-i-1+1]=n:this.modules[8][15-i-1]=n}this.modules[this.moduleCount-8][8]=!t},mapData:function(t,e){for(var r=-1,o=this.moduleCount-1,i=7,n=0,a=this.moduleCount-1;a>0;a-=2)for(6==a&&a--;;){for(var s=0;s<2;s++)if(null==this.modules[o][a-s]){var h=!1;n<t.length&&(h=1==(t[n]>>>i&1)),g.getMask(e,o,a-s)&&(h=!h),this.modules[o][a-s]=h,-1==--i&&(n++,i=7)}if((o+=r)<0||this.moduleCount<=o){o-=r,r=-r;break}}}},e.PAD0=236,e.PAD1=17,e.createData=function(t,r,o){for(var i=m.getRSBlocks(t,r),n=new _,a=0;a<o.length;a++){var s=o[a];n.put(s.mode,4),n.put(s.getLength(),g.getLengthInBits(s.mode,t)),s.write(n)}var h=0;for(a=0;a<i.length;a++)h+=i[a].dataCount;if(n.getLengthInBits()>8*h)throw new Error("code length overflow. ("+n.getLengthInBits()+">"+8*h+")");for(n.getLengthInBits()+4<=8*h&&n.put(0,4);n.getLengthInBits()%8!=0;)n.putBit(!1);for(;!(n.getLengthInBits()>=8*h||(n.put(e.PAD0,8),n.getLengthInBits()>=8*h));)n.put(e.PAD1,8);return e.createBytes(n,i)},e.createBytes=function(t,e){for(var r=0,o=0,i=0,n=new Array(e.length),a=new Array(e.length),s=0;s<e.length;s++){var h=e[s].dataCount,l=e[s].totalCount-h;o=Math.max(o,h),i=Math.max(i,l),n[s]=new Array(h);for(var u=0;u<n[s].length;u++)n[s][u]=255&t.buffer[u+r];r+=h;var f=g.getErrorCorrectPolynomial(l),d=new p(n[s],f.getLength()-1).mod(f);a[s]=new Array(f.getLength()-1);for(u=0;u<a[s].length;u++){var c=u+d.getLength()-a[s].length;a[s][u]=c>=0?d.get(c):0}}var m=0;for(u=0;u<e.length;u++)m+=e[u].totalCount;var _=new Array(m),v=0;for(u=0;u<o;u++)for(s=0;s<e.length;s++)u<n[s].length&&(_[v++]=n[s][u]);for(u=0;u<i;u++)for(s=0;s<e.length;s++)u<a[s].length&&(_[v++]=a[s][u]);return _};for(var r={MODE_NUMBER:1,MODE_ALPHA_NUM:2,MODE_8BIT_BYTE:4,MODE_KANJI:8},o={L:1,M:0,Q:3,H:2},i=0,n=1,a=2,s=3,h=4,l=5,u=6,f=7,g={PATTERN_POSITION_TABLE:[[],[6,18],[6,22],[6,26],[6,30],[6,34],[6,22,38],[6,24,42],[6,26,46],[6,28,50],[6,30,54],[6,32,58],[6,34,62],[6,26,46,66],[6,26,48,70],[6,26,50,74],[6,30,54,78],[6,30,56,82],[6,30,58,86],[6,34,62,90],[6,28,50,72,94],[6,26,50,74,98],[6,30,54,78,102],[6,28,54,80,106],[6,32,58,84,110],[6,30,58,86,114],[6,34,62,90,118],[6,26,50,74,98,122],[6,30,54,78,102,126],[6,26,52,78,104,130],[6,30,56,82,108,134],[6,34,60,86,112,138],[6,30,58,86,114,142],[6,34,62,90,118,146],[6,30,54,78,102,126,150],[6,24,50,76,102,128,154],[6,28,54,80,106,132,158],[6,32,58,84,110,136,162],[6,26,54,82,110,138,166],[6,30,58,86,114,142,170]],G15:1335,G18:7973,G15_MASK:21522,getBCHTypeInfo:function(t){for(var e=t<<10;g.getBCHDigit(e)-g.getBCHDigit(g.G15)>=0;)e^=g.G15<<g.getBCHDigit(e)-g.getBCHDigit(g.G15);return(t<<10|e)^g.G15_MASK},getBCHTypeNumber:function(t){for(var e=t<<12;g.getBCHDigit(e)-g.getBCHDigit(g.G18)>=0;)e^=g.G18<<g.getBCHDigit(e)-g.getBCHDigit(g.G18);return t<<12|e},getBCHDigit:function(t){for(var e=0;0!=t;)e++,t>>>=1;return e},getPatternPosition:function(t){return g.PATTERN_POSITION_TABLE[t-1]},getMask:function(t,e,r){switch(t){case i:return(e+r)%2==0;case n:return e%2==0;case a:return r%3==0;case s:return(e+r)%3==0;case h:return(Math.floor(e/2)+Math.floor(r/3))%2==0;case l:return e*r%2+e*r%3==0;case u:return(e*r%2+e*r%3)%2==0;case f:return(e*r%3+(e+r)%2)%2==0;default:throw new Error("bad maskPattern:"+t)}},getErrorCorrectPolynomial:function(t){for(var e=new p([1],0),r=0;r<t;r++)e=e.multiply(new p([1,d.gexp(r)],0));return e},getLengthInBits:function(t,e){if(1<=e&&e<10)switch(t){case r.MODE_NUMBER:return 10;case r.MODE_ALPHA_NUM:return 9;case r.MODE_8BIT_BYTE:case r.MODE_KANJI:return 8;default:throw new Error("mode:"+t)}else if(e<27)switch(t){case r.MODE_NUMBER:return 12;case r.MODE_ALPHA_NUM:return 11;case r.MODE_8BIT_BYTE:return 16;case r.MODE_KANJI:return 10;default:throw new Error("mode:"+t)}else{if(!(e<41))throw new Error("type:"+e);switch(t){case r.MODE_NUMBER:return 14;case r.MODE_ALPHA_NUM:return 13;case r.MODE_8BIT_BYTE:return 16;case r.MODE_KANJI:return 12;default:throw new Error("mode:"+t)}}},getLostPoint:function(t){for(var e=t.getModuleCount(),r=0,o=0;o<e;o++)for(var i=0;i<e;i++){for(var n=0,a=t.isDark(o,i),s=-1;s<=1;s++)if(!(o+s<0||e<=o+s))for(var h=-1;h<=1;h++)i+h<0||e<=i+h||0==s&&0==h||a==t.isDark(o+s,i+h)&&n++;n>5&&(r+=3+n-5)}for(o=0;o<e-1;o++)for(i=0;i<e-1;i++){var l=0;t.isDark(o,i)&&l++,t.isDark(o+1,i)&&l++,t.isDark(o,i+1)&&l++,t.isDark(o+1,i+1)&&l++,0!=l&&4!=l||(r+=3)}for(o=0;o<e;o++)for(i=0;i<e-6;i++)t.isDark(o,i)&&!t.isDark(o,i+1)&&t.isDark(o,i+2)&&t.isDark(o,i+3)&&t.isDark(o,i+4)&&!t.isDark(o,i+5)&&t.isDark(o,i+6)&&(r+=40);for(i=0;i<e;i++)for(o=0;o<e-6;o++)t.isDark(o,i)&&!t.isDark(o+1,i)&&t.isDark(o+2,i)&&t.isDark(o+3,i)&&t.isDark(o+4,i)&&!t.isDark(o+5,i)&&t.isDark(o+6,i)&&(r+=40);var u=0;for(i=0;i<e;i++)for(o=0;o<e;o++)t.isDark(o,i)&&u++;return r+=10*(Math.abs(100*u/e/e-50)/5)}},d={glog:function(t){if(t<1)throw new Error("glog("+t+")");return d.LOG_TABLE[t]},gexp:function(t){for(;t<0;)t+=255;for(;t>=256;)t-=255;return d.EXP_TABLE[t]},EXP_TABLE:new Array(256),LOG_TABLE:new Array(256)},c=0;c<8;c++)d.EXP_TABLE[c]=1<<c;for(c=8;c<256;c++)d.EXP_TABLE[c]=d.EXP_TABLE[c-4]^d.EXP_TABLE[c-5]^d.EXP_TABLE[c-6]^d.EXP_TABLE[c-8];for(c=0;c<255;c++)d.LOG_TABLE[d.EXP_TABLE[c]]=c;function p(t,e){if(void 0==t.length)throw new Error(t.length+"/"+e);for(var r=0;r<t.length&&0==t[r];)r++;this.num=new Array(t.length-r+e);for(var o=0;o<t.length-r;o++)this.num[o]=t[o+r]}function m(t,e){this.totalCount=t,this.dataCount=e}function _(){this.buffer=[],this.length=0}p.prototype={get:function(t){return this.num[t]},getLength:function(){return this.num.length},multiply:function(t){for(var e=new Array(this.getLength()+t.getLength()-1),r=0;r<this.getLength();r++)for(var o=0;o<t.getLength();o++)e[r+o]^=d.gexp(d.glog(this.get(r))+d.glog(t.get(o)));return new p(e,0)},mod:function(t){if(this.getLength()-t.getLength()<0)return this;for(var e=d.glog(this.get(0))-d.glog(t.get(0)),r=new Array(this.getLength()),o=0;o<this.getLength();o++)r[o]=this.get(o);for(o=0;o<t.getLength();o++)r[o]^=d.gexp(d.glog(t.get(o))+e);return new p(r,0).mod(t)}},m.RS_BLOCK_TABLE=[[1,26,19],[1,26,16],[1,26,13],[1,26,9],[1,44,34],[1,44,28],[1,44,22],[1,44,16],[1,70,55],[1,70,44],[2,35,17],[2,35,13],[1,100,80],[2,50,32],[2,50,24],[4,25,9],[1,134,108],[2,67,43],[2,33,15,2,34,16],[2,33,11,2,34,12],[2,86,68],[4,43,27],[4,43,19],[4,43,15],[2,98,78],[4,49,31],[2,32,14,4,33,15],[4,39,13,1,40,14],[2,121,97],[2,60,38,2,61,39],[4,40,18,2,41,19],[4,40,14,2,41,15],[2,146,116],[3,58,36,2,59,37],[4,36,16,4,37,17],[4,36,12,4,37,13],[2,86,68,2,87,69],[4,69,43,1,70,44],[6,43,19,2,44,20],[6,43,15,2,44,16],[4,101,81],[1,80,50,4,81,51],[4,50,22,4,51,23],[3,36,12,8,37,13],[2,116,92,2,117,93],[6,58,36,2,59,37],[4,46,20,6,47,21],[7,42,14,4,43,15],[4,133,107],[8,59,37,1,60,38],[8,44,20,4,45,21],[12,33,11,4,34,12],[3,145,115,1,146,116],[4,64,40,5,65,41],[11,36,16,5,37,17],[11,36,12,5,37,13],[5,109,87,1,110,88],[5,65,41,5,66,42],[5,54,24,7,55,25],[11,36,12,7,37,13],[5,122,98,1,123,99],[7,73,45,3,74,46],[15,43,19,2,44,20],[3,45,15,13,46,16],[1,135,107,5,136,108],[10,74,46,1,75,47],[1,50,22,15,51,23],[2,42,14,17,43,15],[5,150,120,1,151,121],[9,69,43,4,70,44],[17,50,22,1,51,23],[2,42,14,19,43,15],[3,141,113,4,142,114],[3,70,44,11,71,45],[17,47,21,4,48,22],[9,39,13,16,40,14],[3,135,107,5,136,108],[3,67,41,13,68,42],[15,54,24,5,55,25],[15,43,15,10,44,16],[4,144,116,4,145,117],[17,68,42],[17,50,22,6,51,23],[19,46,16,6,47,17],[2,139,111,7,140,112],[17,74,46],[7,54,24,16,55,25],[34,37,13],[4,151,121,5,152,122],[4,75,47,14,76,48],[11,54,24,14,55,25],[16,45,15,14,46,16],[6,147,117,4,148,118],[6,73,45,14,74,46],[11,54,24,16,55,25],[30,46,16,2,47,17],[8,132,106,4,133,107],[8,75,47,13,76,48],[7,54,24,22,55,25],[22,45,15,13,46,16],[10,142,114,2,143,115],[19,74,46,4,75,47],[28,50,22,6,51,23],[33,46,16,4,47,17],[8,152,122,4,153,123],[22,73,45,3,74,46],[8,53,23,26,54,24],[12,45,15,28,46,16],[3,147,117,10,148,118],[3,73,45,23,74,46],[4,54,24,31,55,25],[11,45,15,31,46,16],[7,146,116,7,147,117],[21,73,45,7,74,46],[1,53,23,37,54,24],[19,45,15,26,46,16],[5,145,115,10,146,116],[19,75,47,10,76,48],[15,54,24,25,55,25],[23,45,15,25,46,16],[13,145,115,3,146,116],[2,74,46,29,75,47],[42,54,24,1,55,25],[23,45,15,28,46,16],[17,145,115],[10,74,46,23,75,47],[10,54,24,35,55,25],[19,45,15,35,46,16],[17,145,115,1,146,116],[14,74,46,21,75,47],[29,54,24,19,55,25],[11,45,15,46,46,16],[13,145,115,6,146,116],[14,74,46,23,75,47],[44,54,24,7,55,25],[59,46,16,1,47,17],[12,151,121,7,152,122],[12,75,47,26,76,48],[39,54,24,14,55,25],[22,45,15,41,46,16],[6,151,121,14,152,122],[6,75,47,34,76,48],[46,54,24,10,55,25],[2,45,15,64,46,16],[17,152,122,4,153,123],[29,74,46,14,75,47],[49,54,24,10,55,25],[24,45,15,46,46,16],[4,152,122,18,153,123],[13,74,46,32,75,47],[48,54,24,14,55,25],[42,45,15,32,46,16],[20,147,117,4,148,118],[40,75,47,7,76,48],[43,54,24,22,55,25],[10,45,15,67,46,16],[19,148,118,6,149,119],[18,75,47,31,76,48],[34,54,24,34,55,25],[20,45,15,61,46,16]],m.getRSBlocks=function(t,e){var r=m.getRsBlockTable(t,e);if(void 0==r)throw new Error("bad rs block @ typeNumber:"+t+"/errorCorrectLevel:"+e);for(var o=r.length/3,i=[],n=0;n<o;n++)for(var a=r[3*n+0],s=r[3*n+1],h=r[3*n+2],l=0;l<a;l++)i.push(new m(s,h));return i},m.getRsBlockTable=function(t,e){switch(e){case o.L:return m.RS_BLOCK_TABLE[4*(t-1)+0];case o.M:return m.RS_BLOCK_TABLE[4*(t-1)+1];case o.Q:return m.RS_BLOCK_TABLE[4*(t-1)+2];case o.H:return m.RS_BLOCK_TABLE[4*(t-1)+3];default:return}},_.prototype={get:function(t){var e=Math.floor(t/8);return 1==(this.buffer[e]>>>7-t%8&1)},put:function(t,e){for(var r=0;r<e;r++)this.putBit(1==(t>>>e-r-1&1))},getLengthInBits:function(){return this.length},putBit:function(t){var e=Math.floor(this.length/8);this.buffer.length<=e&&this.buffer.push(0),t&&(this.buffer[e]|=128>>>this.length%8),this.length++}};var v=[[17,14,11,7],[32,26,20,14],[53,42,32,24],[78,62,46,34],[106,84,60,44],[134,106,74,58],[154,122,86,64],[192,152,108,84],[230,180,130,98],[271,213,151,119],[321,251,177,137],[367,287,203,155],[425,331,241,177],[458,362,258,194],[520,412,292,220],[586,450,322,250],[644,504,364,280],[718,560,394,310],[792,624,442,338],[858,666,482,382],[929,711,509,403],[1003,779,565,439],[1091,857,611,461],[1171,911,661,511],[1273,997,715,535],[1367,1059,751,593],[1465,1125,805,625],[1528,1190,868,658],[1628,1264,908,698],[1732,1370,982,742],[1840,1452,1030,790],[1952,1538,1112,842],[2068,1628,1168,898],[2188,1722,1228,958],[2303,1809,1283,983],[2431,1911,1351,1051],[2563,1989,1423,1093],[2699,2099,1499,1139],[2809,2213,1579,1219],[2953,2331,1663,1273]];function A(t,e){for(var r=1,i=function(t){var e=encodeURI(t).toString().replace(/\%[0-9a-fA-F]{2}/g,"a");return e.length+(e.length!=t.length?3:0)}(t),n=0,a=v.length;n<a;n++){var s=0;switch(e){case o.L:s=v[n][0];break;case o.M:s=v[n][1];break;case o.Q:s=v[n][2];break;case o.H:s=v[n][3]}if(i<=s)break;r++}if(r>v.length)throw new Error("Too long data");return r}
return function(text) { var code = new e(A(text, o.M), o.M); code.addData(text); code.make(); return code; };
})();


function textValue(value) {
	return value == null ? '' : String(value);
}

function byteLength(value) {
	return new TextEncoder().encode(value).length;
}

function encryptionMode(encryption) {
	var value = textValue(encryption);
	if (value === 'none') return value;
	// These cipher names and aliases match the existing LuCI expert editor.
	var match = /^(psk-mixed|psk2|sae-mixed|sae)(?:\+(?:ccmp|aes|ccmp256|gcmp|gcmp256|tkip|tkip\+(?:ccmp|aes)|(?:ccmp|aes)\+tkip))?$/.exec(value);
	return match ? match[1] : '';
}

function personal(encryption) {
	var mode = encryptionMode(encryption);
	return !!mode && mode !== 'none';
}

function cloneBasic(profile) {
	return {
		enabled: profile.enabled, ssid: profile.ssid,
		encryption: profile.encryption, key: profile.key
	};
}

function sameBasic(a, b) {
	return a.enabled === b.enabled && a.ssid === b.ssid &&
		a.encryption === b.encryption && a.key === b.key;
}

function wifiQrPayload(iface) {
	var mode = encryptionMode(iface.encryption || 'none');
	if (!mode) throw new Error('当前加密方式不支持扫码连接。');
	function escape(value) { return textValue(value).replace(/[\\;,:"]/g, function(ch) { return '\\' + ch; }); }
	var type = mode === 'none' ? 'nopass' : mode === 'sae' ? 'SAE' : 'WPA';
	return 'WIFI:T:' + type + ';S:' + escape(iface.ssid) + ';' +
		(mode === 'none' ? '' : 'P:' + escape(iface.key) + ';') +
		(iface.hidden === '1' ? 'H:true;' : '') + ';';
}

function wifiQrSvg(payload) {
	var code = wifiQrMatrix(payload), count = code.getModuleCount(), cells = [];
	for (var row = 0; row < count; row++)
		for (var col = 0; col < count; col++)
			if (code.isDark(row, col)) cells.push('M' + col + ' ' + row + 'h1v1h-1z');
	var ns = 'http://www.w3.org/2000/svg';
	var svg = document.createElementNS(ns, 'svg');
	svg.setAttribute('viewBox', '-4 -4 ' + (count + 8) + ' ' + (count + 8));
	svg.setAttribute('width', '240');
	svg.setAttribute('height', '240');
	svg.setAttribute('role', 'img');
	svg.setAttribute('aria-label', 'Wi-Fi 连接二维码');
	svg.setAttribute('shape-rendering', 'crispEdges');
	var background = document.createElementNS(ns, 'rect');
	background.setAttribute('x', '-4'); background.setAttribute('y', '-4');
	background.setAttribute('width', String(count + 8)); background.setAttribute('height', String(count + 8));
	background.setAttribute('fill', '#fff');
	var path = document.createElementNS(ns, 'path');
	path.setAttribute('fill', '#000'); path.setAttribute('d', cells.join(''));
	svg.appendChild(background); svg.appendChild(path);
	return svg;
}

function wifiQrControl(profiles) {
	var popup = E('div', { 'class': 'wifi-qr-popup', id: 'wifi-qr-' + (++nextId), hidden: '', role: 'region', 'aria-label': '扫码连接 Wi-Fi' });
	var button = E('button', { type: 'button', 'class': 'wifi-qr-button', 'aria-label': '展示 Wi-Fi 连接二维码', 'aria-expanded': 'false', 'aria-controls': popup.id });
	var ns = 'http://www.w3.org/2000/svg';
	var icon = document.createElementNS(ns, 'svg');
	icon.setAttribute('viewBox', '0 0 24 24'); icon.setAttribute('aria-hidden', 'true');
	var path = document.createElementNS(ns, 'path');
	path.setAttribute('fill', 'currentColor');
	path.setAttribute('fill-rule', 'evenodd');
	path.setAttribute('d', 'M1 1h9v9H1zM3 3v5h5V3zM4 4h3v3H4zM14 1h9v9h-9zM16 3v5h5V3zM17 4h3v3h-3zM1 14h9v9H1zM3 16v5h5v-5zM4 17h3v3H4zM12 12h4v4h-4zM18 12h5v3h-5zM12 18h3v5h-3zM17 17h3v3h-3zM21 17h2v6h-5v-2h3z');
	icon.appendChild(path); button.appendChild(icon);
	var wrap = E('span', { 'class': 'wifi-qr' }, [ button, popup ]);
	var open = false, pinned = false, request = 0;
	function close() {
		open = pinned = false; request++;
		popup.hidden = true; button.setAttribute('aria-expanded', 'false');
		dom.content(popup, []);
		document.removeEventListener('click', outside);
		document.removeEventListener('keydown', escapeKey);
		document.removeEventListener('uci-applied', close);
		document.removeEventListener('uci-reverted', close);
		if (closeWifiQr === close) closeWifiQr = null;
	}
	function outside(ev) { if (!wrap.contains(ev.target)) close(); }
	function escapeKey(ev) { if (ev.key === 'Escape') close(); }
	function show() {
		if (open) return;
		if (closeWifiQr) closeWifiQr();
		open = true; closeWifiQr = close;
		var current = ++request;
		popup.hidden = false; button.setAttribute('aria-expanded', 'true');
		dom.content(popup, E('p', { 'class': 'wifi-hint' }, '正在读取 Wi-Fi…'));
		document.addEventListener('click', outside);
		document.addEventListener('keydown', escapeKey);
		document.addEventListener('uci-applied', close);
		document.addEventListener('uci-reverted', close);
		Promise.all([ callWifiConfig('wireless'), uci.changes() ]).then(function(data) {
			if (!open || current !== request) return;
			if (data[1].wireless && data[1].wireless.length)
				throw new Error('请先保存并应用待生效的无线设置。');
			var configs = profiles.map(function(profile) {
				var iface = data[0][profile.iface], radio = data[0][profile.radio];
				if (!iface || !radio || iface.device !== profile.radio || !iface.ssid)
					throw new Error('未读取到此频段的 Wi-Fi 配置。');
				if (iface.disabled === '1' || radio.disabled === '1') throw new Error('此 Wi-Fi 已关闭。');
				return iface;
			});
			var payload = wifiQrPayload(configs[0]);
			if (configs.some(function(iface) { return wifiQrPayload(iface) !== payload; }))
				throw new Error('双频配置尚未一致，请先保存并应用。');
			dom.content(popup, [ E('strong', { 'class': 'wifi-qr-name' }, configs[0].ssid), wifiQrSvg(payload), E('p', { 'class': 'wifi-hint' }, '用手机系统扫码连接') ]);
		}).catch(function(error) {
			if (open && current === request) dom.content(popup, E('p', { 'class': 'wifi-hint' }, error.message || '二维码读取失败，请重试。'));
		});
	}
	wrap.addEventListener('mouseenter', show);
	wrap.addEventListener('mouseleave', function() { if (!pinned && !wrap.contains(document.activeElement)) close(); });
	wrap.addEventListener('focusout', function(ev) { if (!pinned && !wrap.contains(ev.relatedTarget)) close(); });
	button.addEventListener('focus', show);
	button.addEventListener('click', function() { if (pinned) close(); else { show(); pinned = true; } });
	return wrap;
}

function bandOf(radio, device) {
	if ([ '2g', '5g', '6g', '60g' ].indexOf(radio.band) >= 0)
		return radio.band;
	var frequency = device ? Number(device.ubus('dev', 'iwinfo', 'frequency')) : 0;
	if (frequency >= 2400 && frequency < 2500) return '2g';
	if (frequency >= 4900 && frequency < 5925) return '5g';
	if (frequency >= 5925 && frequency < 7126) return '6g';
	if (frequency >= 57000) return '60g';
	if (radio.hwmode === '11g' || radio.hwmode === '11b') return '2g';
	if (radio.hwmode === '11a') return '5g';
	return '';
}

function bandLabel(band) {
	return { '2g': '2.4 GHz', '5g': '5 GHz', '6g': '6 GHz', '60g': '60 GHz' }[band] || '无线网络';
}

function frequencyBand(mhz, band) {
	if ([ 2, 5, 6, 60 ].indexOf(Number(band)) >= 0)
		return Number(band) + 'g';
	if (mhz >= 2400 && mhz < 2500) return '2g';
	if (mhz >= 4900 && mhz < 5925) return '5g';
	if (mhz >= 5925 && mhz < 7126) return '6g';
	if (mhz >= 57000) return '60g';
	return '';
}

function frequencyList(profile, frequencies) {
	if (Array.isArray(frequencies) && frequencies.length) return frequencies;
	if (profile.type !== 'mtwifi') return [];
	// Match the MTK expert view when an inactive radio cannot report its list.
	var channels = profile.band === '2g'
		? [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 ]
		: profile.band === '5g'
			? [ 36, 40, 44, 48, 52, 56, 60, 64, 149, 153, 157, 161, 165 ] : [];
	return channels.map(function(channel) {
		return { channel: channel, mhz: profile.band === '2g' ? 2407 + channel * 5 : 5000 + channel * 5 };
	});
}

function field(label, control, hint) {
	var target = control.matches('input, select, button') ? control : control.querySelector('input, select, button');
	var id = target.id || ('myos-wifi-' + (++nextId));
	target.id = id;
	var content = E('div', { 'class': 'wifi-control' }, [ control ]);
	if (hint) content.appendChild(E('p', { 'class': 'wifi-hint' }, hint));
	return E('div', { 'class': 'wifi-field' }, [
		E('label', { 'class': 'wifi-label', 'for': id }, label), content
	]);
}

function toggle(value, label, readonly, onChange) {
	var input = E('input', {
		type: 'checkbox', checked: value ? '' : null, disabled: readonly ? '' : null,
		'aria-label': label,
		change: function() { onChange(this.checked); }
	});
	return E('label', { 'class': 'wifi-switch' }, [
		input, E('span', { 'class': 'wifi-switch-track', 'aria-hidden': 'true' }),
		E('span', { 'class': 'wifi-switch-label' }, label)
	]);
}

function select(value, choices, readonly, onChange) {
	var list = choices.slice();
	if (!list.some(function(choice) { return String(choice[0]) === value; }))
		list.push([ value, value ? value + '（当前）' : '驱动默认' ]);
	return E('select', {
		'class': 'cbi-input-select', disabled: readonly ? '' : null,
		change: function() { onChange(this.value); }
	}, list.map(function(choice) {
		return E('option', { value: String(choice[0]), selected: String(choice[0]) === value ? '' : null, disabled: choice[2] ? '' : null }, choice[1]);
	}));
}

function encryptionChoices(profiles) {
	var choices = [ [ 'psk-mixed', 'WPA / WPA2 个人版' ], [ 'psk2', 'WPA2 个人版' ] ];
	var hasSAE = profiles.every(function(profile) {
		return profile.type === 'mtwifi' || L.hasSystemFeature('hostapd', 'sae');
	});
	if (hasSAE) {
		choices.push([ 'sae-mixed', 'WPA2 / WPA3 个人版' ]);
		choices.push([ 'sae', 'WPA3 个人版' ]);
	}
	choices.push([ 'none', '开放网络（无密码）' ]);
	return choices;
}

function modeLabel(mode) {
	var match = /^(EHT|HE|VHT|HT)(\d+)$/.exec(mode);
	if (!match) return mode || '传统模式';
	return { EHT: 'Wi-Fi 7', HE: 'Wi-Fi 6', VHT: 'Wi-Fi 5', HT: 'Wi-Fi 4' }[match[1]] + ' · ' + match[2] + ' MHz';
}

return baseclass.extend({
	render: function() {
		this.readonly = !L.hasViewPermission();
		this.busy = false;
		this.advanced = [];
		return network.getWifiDevices().then(L.bind(function(devices) {
			var ifaces = uci.sections('wireless', 'wifi-iface');
			var radios = uci.sections('wireless', 'wifi-device');
			this.profiles = [];
			this.unmanaged = 0;
			radios.forEach(L.bind(function(radio) {
				var candidates = ifaces.filter(function(iface) {
					return iface.device === radio['.name'] && (iface.mode === 'ap' || iface.mode === 'ap-wds');
				});
				var lan = candidates.filter(function(iface) { return L.toArray(iface.network).indexOf('lan') >= 0; });
				var iface = lan.length === 1 ? lan[0] : candidates.length === 1 ? candidates[0] : null;
				if (!iface) { this.unmanaged += candidates.length; return; }
				this.unmanaged += candidates.length - 1;
				var device = devices.filter(function(item) { return item.getName() === radio['.name']; })[0];
				var profile = {
					radio: radio['.name'], iface: iface['.name'], type: radio.type,
					band: bandOf(radio, device), device: device,
					enabled: radio.disabled !== '1' && iface.disabled !== '1',
					ssid: textValue(iface.ssid), encryption: textValue(iface.encryption) || 'none',
					key: personal(iface.encryption || '') ? textValue(iface.key) : '',
					original: {}, draft: {}, frequencies: [], powers: []
				};
				profile.original.basic = cloneBasic(profile);
				profile.draft = cloneBasic(profile);
				[ 'channel', 'htmode', 'txpower' ].forEach(function(option) {
					profile.original[option] = textValue(radio[option]);
					profile[option] = textValue(radio[option]);
				});
				[ 'hidden', 'isolate', 'maxassoc' ].forEach(function(option) {
					profile.original[option] = textValue(iface[option]);
					profile[option] = textValue(iface[option]);
				});
				this.profiles.push(profile);
			}, this));
			this.profiles.sort(function(a, b) {
				return [ '2g', '5g', '6g', '60g', '' ].indexOf(a.band) - [ '2g', '5g', '6g', '60g', '' ].indexOf(b.band);
			});
			var two = this.profiles.filter(function(p) { return p.band === '2g'; });
			var five = this.profiles.filter(function(p) { return p.band === '5g'; });
			this.pair = two.length === 1 && five.length === 1 ? [ two[0], five[0] ] : [];
			// Enterprise, WEP and mesh-specific credentials remain in the expert view.
			if (!this.pair.every(function(p) { return p.encryption === 'none' || personal(p.encryption); })) this.pair = [];
			this.merged = this.pair.length === 2 && sameBasic(this.pair[0].draft, this.pair[1].draft);
			this.shared = this.pair.length ? cloneBasic(this.pair[1].draft) : null;
			return Promise.all(this.profiles.map(function(profile) {
				return Promise.all([
					L.resolveDefault(callFrequencies(profile.radio), []),
					profile.type === 'mtwifi' ? Promise.resolve([]) : L.resolveDefault(callPowers(profile.radio), [])
				]).then(function(data) {
					profile.frequencies = frequencyList(profile, data[0]);
					profile.frequencyFallback = (!Array.isArray(data[0]) || !data[0].length) && profile.frequencies.length > 0;
					profile.powers = data[1];
				});
			}));
		}, this)).then(L.bind(this.renderPage, this));
	},

	renderPage: function() {
		var expert = L.url('admin/network/wireless') + '?expert=1';
		this.cards = E('div', { 'class': 'wifi-cards' });
		this.message = E('div', { 'class': 'wifi-feedback', role: 'status', 'aria-live': 'polite' });
		this.saveButton = E('button', {
			type: 'button', 'class': 'cbi-button cbi-button-apply wifi-save',
			disabled: this.readonly || !this.profiles.length ? '' : null,
			click: L.bind(this.save, this)
		}, '保存并应用');
		var sections = [
			E('link', { rel: 'stylesheet', href: L.resource('tools/myos-wifi.css') + '?v=20261101wifiqr1' }),
			E('div', { 'class': 'wifi-heading' }, [
				E('div', {}, [ E('h2', {}, 'Wi-Fi 设置'), E('p', {}, '轻松设置无线名称、密码和双频连接。') ]),
				E('a', { 'class': 'wifi-expert', href: expert }, '专家设置 ↗')
			])
		];
		if (this.pair.length === 2) {
			sections.push(E('section', { 'class': 'wifi-combine wifi-card' }, [
				E('div', {}, [ E('h3', {}, '双频合一'), E('p', { 'class': 'wifi-hint' }, '2.4 GHz 和 5 GHz 使用相同名称、密码，由终端选择频段。') ]),
					toggle(this.merged, '双频合一', this.readonly, L.bind(function(value) {
					if (value) {
						this.shared = cloneBasic(this.pair[1].draft);
						this.shared.ssid = this.shared.ssid.replace(/[-_ ](?:2\.4G|5G)$/i, '');
						this.shared.enabled = this.pair.some(function(profile) { return profile.draft.enabled; });
					} else {
						this.pair.forEach(L.bind(function(profile) {
							profile.draft = cloneBasic(this.shared);
							var suffix = profile.band === '2g' ? '-2.4G' : '-5G';
							var base = Array.from(this.shared.ssid.replace(/[-_ ](?:2\.4G|5G)$/i, ''));
							while (byteLength(base.join('') + suffix) > 32) base.pop();
							profile.draft.ssid = base.join('') + suffix;
						}, this));
					}
					this.merged = value;
					this.renderCards();
				}, this))
			]));
		}
		sections.push(this.cards);
		if (!this.profiles.length) sections.push(E('p', { 'class': 'wifi-empty' }, '暂无可简化配置的无线接入点，请进入专家设置。'));
		if (this.unmanaged) sections.push(E('p', { 'class': 'wifi-hint' }, '访客、多 SSID 等其他无线网络可在专家设置中管理。'));
		sections.push(this.message, E('div', { 'class': 'wifi-actions' }, [
			E('span', { 'class': 'wifi-hint' }, '修改名称或密码后，无线设备需要重新连接。'), this.saveButton
		]));
		this.root = E('div', { id: 'myos-simple-wifi' }, sections);
		this.renderCards();
		return this.root;
	},

	renderCards: function() {
		if (closeWifiQr) closeWifiQr();
		var cards = [], handled = [];
		if (this.merged) {
			cards.push(this.renderCard(this.shared, this.pair, '双频 Wi-Fi', '2.4 GHz + 5 GHz'));
			handled = this.pair;
		}
		this.profiles.forEach(L.bind(function(profile) {
			if (handled.indexOf(profile) < 0)
				cards.push(this.renderCard(profile.draft, [ profile ], bandLabel(profile.band) + ' Wi-Fi', profile.band === '2g' ? '覆盖更远' : profile.band === '5g' ? '高速连接' : '独立频段'));
		}, this));
		this.cards.classList.toggle('wifi-cards-split', !this.merged && cards.length > 1);
		dom.content(this.cards, cards);
	},

	renderCard: function(draft, profiles, title, subtitle) {
		var mode = encryptionMode(draft.encryption), supported = !!mode;
		var enabledBadge = E('span', { 'class': 'wifi-badge' + (draft.enabled ? ' is-on' : '') }, draft.enabled ? '已开启' : '已关闭');
		var enabledControl = toggle(draft.enabled, 'Wi-Fi 开关', this.readonly, function(value) {
			draft.enabled = value;
			enabledBadge.classList.toggle('is-on', value);
			enabledBadge.textContent = value ? '已开启' : '已关闭';
		});
		var ssid = E('input', {
			type: 'text', 'class': 'cbi-input-text', value: draft.ssid, autocomplete: 'off',
			disabled: this.readonly ? '' : null, 'aria-label': title + ' 名称',
			input: function() { draft.ssid = this.value; }
		});
		var password = E('input', {
			type: 'password', 'class': 'cbi-input-text', value: draft.key, autocomplete: 'new-password',
			disabled: this.readonly || !supported ? '' : null, 'aria-label': title + ' 密码',
			placeholder: draft.encryption === 'none' ? '填写密码可启用 WPA / WPA2 加密' : '输入 Wi-Fi 密码',
			input: function() {
				draft.key = this.value;
				if (draft.encryption === 'none' && this.value) {
					draft.encryption = 'psk-mixed';
					encryption.value = 'psk-mixed';
					passwordHint.textContent = '8–63 字节；支持 WPA2 的 64 位十六进制密钥。';
				}
			}
		});
		var reveal = E('button', {
			type: 'button', 'class': 'wifi-reveal', 'aria-label': '显示密码', 'aria-pressed': 'false',
			disabled: !supported ? '' : null,
			click: function() {
				var visible = password.type === 'password';
				password.type = visible ? 'text' : 'password';
				this.textContent = visible ? '隐藏' : '显示';
				this.setAttribute('aria-pressed', String(visible));
				this.setAttribute('aria-label', visible ? '隐藏密码' : '显示密码');
			}
		}, '显示');
		var passwordHint = E('p', { 'class': 'wifi-hint' }, !supported ? '当前认证方式请通过专家设置管理。' : draft.encryption === 'none' ? '当前为开放网络，填写密码后启用加密。' : '8–63 字节；支持 WPA2 的 64 位十六进制密钥。');
		var encryption = supported ? select(mode, encryptionChoices(profiles), this.readonly, function(value) {
			// Display the authentication family while preserving an unchanged cipher.
			if (value !== encryptionMode(draft.encryption)) draft.encryption = value;
			if (value === 'none') { draft.key = ''; password.value = ''; }
			passwordHint.textContent = value === 'none' ? '当前为开放网络，填写密码后启用加密。' : '8–63 字节；WPA3 密码最多 63 字节。';
		}) : select('expert', [ [ 'expert', '请通过专家设置管理' ] ], true, function() {});
		var basic = E('div', { 'class': 'wifi-basic' }, [
			field('Wi-Fi 开关', enabledControl), field('Wi-Fi 名称', ssid),
			field('Wi-Fi 密码', E('div', { 'class': 'wifi-password' }, [ password, reveal ]))
		]);
		basic.lastElementChild.lastElementChild.appendChild(passwordHint);
		var advanced = E('div', { 'class': 'wifi-advanced-content' }, [ field('加密方式', encryption) ]);
		profiles.forEach(L.bind(function(profile) { advanced.appendChild(this.renderAdvanced(profile, profiles.length > 1)); }, this));
		return E('section', { 'class': 'wifi-card' }, [
			E('div', { 'class': 'wifi-card-heading' }, [
				E('div', { 'class': 'wifi-card-intro' }, [ E('h3', {}, title), E('div', { 'class': 'wifi-card-subtitle' }, [ E('span', { 'class': 'wifi-hint' }, subtitle), wifiQrControl(profiles) ]) ]), enabledBadge
			]),
			basic,
			E('details', { 'class': 'wifi-advanced' }, [ E('summary', {}, [ E('span', {}, '无线高级设置'), E('span', { 'class': 'wifi-chevron', 'aria-hidden': 'true' }, '⌄') ]), advanced ])
		]);
	},

	renderAdvanced: function(profile, showTitle) {
		var controls = [];
		if (showTitle) controls.push(E('h4', {}, bandLabel(profile.band)));
		var channels = [ [ 'auto', '自动' ] ];
		profile.frequencies.forEach(function(item) {
			// Older iwinfo has only restricted; newer replies distinguish no-IR.
			if (!(item.restricted && (item.no_ir == null || item.no_ir)) && frequencyBand(item.mhz, item.band) === profile.band)
				channels.push([ String(item.channel), String(item.channel) + (item.mhz ? ' · ' + item.mhz + ' MHz' : '') ]);
		});
		var channelHint = profile.frequencyFallback ? '暂未读取到实时信道，显示兼容选项。' : channels.length === 1 ? '暂未读取到可用信道，可在专家设置中查看。' : null;
		controls.push(field('信道', select(profile.channel || 'auto', channels, this.readonly, function(value) { profile.channel = value; }), channelHint));
		var modes = profile.device ? L.toArray(profile.device.getHTModes()) : [];
		var maxWidth = profile.band === '2g' ? 40 : profile.band === '5g' ? 160 : null;
		modes = modes.filter(function(mode) {
			var width = /^(EHT|HE|VHT|HT)(\d+)$/.exec(mode);
			return !width || !maxWidth || Number(width[2]) <= maxWidth;
		});
		var modeChoices = modes.map(function(mode) { return [ mode, modeLabel(mode) ]; });
		if (!modeChoices.some(function(choice) { return choice[0] === profile.htmode; })) {
			var configured = /^(EHT|HE|VHT|HT)(\d+)$/.exec(profile.htmode);
			var unsupported = configured && maxWidth && Number(configured[2]) > maxWidth;
			modeChoices.push([ profile.htmode, unsupported ? '当前配置（请重新选择带宽）' : modeLabel(profile.htmode) + '（当前）', unsupported ]);
		}
		controls.push(field('模式与带宽', select(profile.htmode, modeChoices, this.readonly, function(value) { profile.htmode = value; })));
		var powers = [ [ '', '驱动默认' ] ];
		if (profile.type === 'mtwifi')
			[ 100, 75, 50, 25, 10 ].forEach(function(value) { powers.push([ String(value), value + '%' ]); });
		else profile.powers.forEach(function(item) { powers.push([ String(item.dbm), item.dbm + ' dBm · ' + item.mw + ' mW' ]); });
		controls.push(field('发射功率', select(profile.txpower, powers, this.readonly, function(value) { profile.txpower = value; })));
		controls.push(field('隐藏名称', toggle(profile.hidden === '1', '隐藏 Wi-Fi 名称', this.readonly, function(value) { profile.hidden = value ? '1' : '0'; })));
		controls.push(field('终端隔离', toggle(profile.isolate === '1', '隔离无线终端', this.readonly, function(value) { profile.isolate = value ? '1' : '0'; })));
		controls.push(field('最大接入数', E('input', {
			type: 'number', min: '0', step: '1', 'class': 'cbi-input-text', value: profile.maxassoc,
			placeholder: '驱动默认', disabled: this.readonly ? '' : null,
			input: function() { profile.maxassoc = this.value; }
		}), '留空使用驱动默认值。'));
		return E('div', { 'class': 'wifi-radio-advanced' }, controls);
	},

	validate: function(profile, draft) {
		if (draft.enabled || draft.ssid !== profile.original.basic.ssid) {
			if (byteLength(draft.ssid) < 1 || byteLength(draft.ssid) > 32 || /[\x00-\x1f\x7f]/.test(draft.ssid))
				throw new Error(bandLabel(profile.band) + '：Wi-Fi 名称须为 1–32 字节，且不能包含控制字符。');
		}
		if (personal(draft.encryption) && (draft.enabled || draft.key !== profile.original.basic.key || draft.encryption !== profile.original.basic.encryption)) {
			var bytes = byteLength(draft.key), rawPSK = /^[0-9a-f]{64}$/i.test(draft.key) && draft.encryption.indexOf('sae') !== 0;
			if ((!rawPSK && (bytes < 8 || bytes > 63)) || /[\x00-\x1f\x7f]/.test(draft.key))
				throw new Error(bandLabel(profile.band) + '：Wi-Fi 密码须为 8–63 字节；WPA2 也支持 64 位十六进制密钥。');
		}
		if (profile.maxassoc !== profile.original.maxassoc && profile.maxassoc !== '' && !/^\d+$/.test(profile.maxassoc))
			throw new Error(bandLabel(profile.band) + '：最大接入数须为非负整数。');
	},

	stage: function(profile, draft) {
		var original = profile.original.basic, changes = 0;
		function set(section, option, value) {
			uci.set('wireless', section, option, value === '' ? null : value);
			changes++;
		}
		if (draft.enabled !== original.enabled) {
			set(profile.radio, 'disabled', draft.enabled ? '0' : '1');
			if (draft.enabled) set(profile.iface, 'disabled', '0');
		}
		if (draft.ssid !== original.ssid) set(profile.iface, 'ssid', draft.ssid);
		if (draft.encryption !== original.encryption) {
			set(profile.iface, 'encryption', draft.encryption);
			if (draft.encryption === 'sae') set(profile.iface, 'ieee80211w', '2');
			else if (draft.encryption === 'sae-mixed' && uci.get('wireless', profile.iface, 'ieee80211w') !== '2') set(profile.iface, 'ieee80211w', '1');
			else if (original.encryption.indexOf('sae') === 0 && draft.encryption.indexOf('sae') !== 0) set(profile.iface, 'ieee80211w', '');
		}
		if (personal(draft.encryption) && (draft.key !== original.key || draft.encryption !== original.encryption)) set(profile.iface, 'key', draft.key);
		else if (draft.encryption === 'none' && original.encryption !== 'none') set(profile.iface, 'key', '');
		[ 'channel', 'htmode', 'txpower', 'hidden', 'isolate', 'maxassoc' ].forEach(function(option) {
			if (profile[option] !== profile.original[option])
				set([ 'channel', 'htmode', 'txpower' ].indexOf(option) >= 0 ? profile.radio : profile.iface, option, profile[option]);
		});
		return changes;
	},

	setBusy: function(value) {
		this.busy = value;
		if (value) {
			this.disabledStates = Array.from(this.root.querySelectorAll('input, select, button')).map(function(control) {
				var state = [ control, control.disabled ];
				control.disabled = true;
				return state;
			});
		} else {
			(this.disabledStates || []).forEach(function(state) { state[0].disabled = state[1]; });
		}
		this.saveButton.classList.toggle('spinning', value);
	},

	save: function() {
		if (this.busy || this.readonly) return;
		this.message.textContent = '';
		var drafts = this.profiles.map(L.bind(function(profile) {
			return cloneBasic(this.merged && this.pair.indexOf(profile) >= 0 ? this.shared : profile.draft);
		}, this));
		var snapshots = this.profiles.map(function(profile) { return Object.assign({}, profile); });
		try {
			snapshots.forEach(L.bind(function(profile, index) { this.validate(profile, drafts[index]); }, this));
		} catch (error) {
			this.message.classList.add('is-error');
			this.message.textContent = error.message;
			return;
		}
		var count = 0;
		snapshots.forEach(L.bind(function(profile, index) { count += this.stage(profile, drafts[index]); }, this));
		this.setBusy(true);
		if (!count) {
			return uci.changes().then(L.bind(function(changes) {
				if (changes.wireless && changes.wireless.length) {
					return ui.changes.init().then(function() { ui.changes.apply(true); });
				}
				this.message.classList.remove('is-error');
				this.message.textContent = '当前设置未更改。';
			}, this)).catch(L.bind(function(error) {
				this.message.classList.add('is-error');
				this.message.textContent = '读取待应用设置失败：' + error.message;
			}, this)).finally(L.bind(function() { this.setBusy(false); }, this));
		}
		return uci.save().then(L.bind(function() {
			// Native LuCI apply retains connectivity checks and automatic rollback.
			this.profiles.forEach(function(profile, index) {
				profile.original.basic = cloneBasic(drafts[index]);
				[ 'channel', 'htmode', 'txpower', 'hidden', 'isolate', 'maxassoc' ].forEach(function(option) { profile.original[option] = snapshots[index][option]; });
			});
			return ui.changes.init();
		}, this)).then(function() {
			ui.changes.apply(true);
		}).catch(L.bind(function(error) {
			this.message.classList.add('is-error');
			this.message.textContent = '保存失败：' + error.message;
		}, this)).finally(L.bind(function() {
			// apply() owns the modal and returns void in this LuCI build.
			this.setBusy(false);
		}, this));
	}
});
