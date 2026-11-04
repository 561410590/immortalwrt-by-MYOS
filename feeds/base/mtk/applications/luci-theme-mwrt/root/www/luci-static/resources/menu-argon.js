'use strict';'require baseclass';'require ui';return baseclass.extend({__init__:function(){ui.menu.load().then(L.bind(this.render,this));},render:function(tree){var node=tree,url='',children=ui.menu.getChildren(tree);for(var i=0;i<children.length;i++){var isActive=(L.env.requestpath.length?children[i].name==L.env.requestpath[0]:i==0);if(isActive)
this.renderMainMenu(children[i],children[i].name);}
if(L.env.dispatchpath.length>=3){for(var i=0;i<3&&node;i++){node=node.children[L.env.dispatchpath[i]];url=url+(url?'/':'')+L.env.dispatchpath[i];}
if(node)
this.renderTabMenu(node,url);}
document.querySelector('a.showSide').addEventListener('click',L.bind(this.handleSidebarToggle,this));document.querySelector('.darkMask').addEventListener('click',L.bind(this.handleSidebarToggle,this));},handleMenuExpand:function(ev){
var a=ev.currentTarget,slide_menu=a.nextElementSibling;
if(!slide_menu||!slide_menu.classList.contains('slide-menu'))return;
ev.preventDefault();ev.stopPropagation();
var collapse=a.hasAttribute('aria-expanded')?a.getAttribute('aria-expanded')==='true':slide_menu.classList.contains('active');
document.querySelectorAll('#mainmenu > .nav > li > .slide-menu').forEach(function(ul){
 if(ul===slide_menu)return;
 var link=ul.previousElementSibling;
 $(ul).stop(true,true).slideUp(160,function(){ul.classList.remove('active');});
 link.classList.remove('active');link.setAttribute('aria-expanded','false');
});
$(slide_menu).stop(true,true);
a.setAttribute('aria-expanded',String(!collapse));
a.classList.toggle('active',!collapse);
if(collapse)$(slide_menu).slideUp(160,function(){slide_menu.classList.remove('active');});
else {slide_menu.classList.add('active');$(slide_menu).hide().slideDown(160);}
a.blur();
},renderMainMenu:function(tree,url,level){var l=(level||0)+1,ul=E('ul',{'class':level?'slide-menu':'nav'}),children=ui.menu.getChildren(tree);if(children.length==0||l>2)
return E([]);for(var i=0;i<children.length;i++){var isActive=((L.env.dispatchpath[l]==children[i].name)&&(L.env.dispatchpath[l-1]==tree.name)),submenu=this.renderMainMenu(children[i],url+'/'+children[i].name,l),hasChildren=submenu.children.length,slideClass=hasChildren?'slide':null,menuClass=hasChildren?'menu':'food';if(isActive){ul.classList.add('active');slideClass+=" active";menuClass+=" active";}
ul.appendChild(E('li',{'class':slideClass},[E('a',{'href':L.url(url,children[i].name),'click':(l==1)?L.bind(this.handleMenuExpand,this):null,'class':menuClass,'data-title':hasChildren?children[i].title.replace(" ","_"):children[i].title.replace(" ","_"),},[_(children[i].title)]),submenu]));}
if(l==1){document.querySelector('#mainmenu').appendChild(ul);document.querySelector('#mainmenu').style.display='';}
return ul;},renderTabMenu:function(tree,url,level){var container=document.querySelector('#tabmenu'),l=(level||0)+1,ul=E('ul',{'class':'tabs'}),children=ui.menu.getChildren(tree),activeNode=null;if(children.length==0)
return E([]);for(var i=0;i<children.length;i++){var isActive=(L.env.dispatchpath[l+2]==children[i].name),activeClass=isActive?' active':'',className='tabmenu-item-%s %s'.format(children[i].name,activeClass);ul.appendChild(E('li',{'class':className},[E('a',{'href':L.url(url,children[i].name)},[_(children[i].title)])]));if(isActive)
activeNode=children[i];}
container.appendChild(ul);container.style.display='';if(activeNode)
container.appendChild(this.renderTabMenu(activeNode,url+'/'+activeNode.name,l));return ul;},handleSidebarToggle:function(ev){var showside=document.querySelector('a.showSide'),sidebar=document.querySelector('#mainmenu'),darkmask=document.querySelector('.darkMask'),scrollbar=document.querySelector('.main-right');if(showside.classList.contains('active')){showside.classList.remove('active');sidebar.classList.remove('active');scrollbar.classList.remove('active');darkmask.classList.remove('active');}
else{showside.classList.add('active');sidebar.classList.add('active');scrollbar.classList.add('active');darkmask.classList.add('active');}}});
