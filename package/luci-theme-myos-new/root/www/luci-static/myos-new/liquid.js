/* A single frosted material follows browser accessibility preferences. */
(() => {
 'use strict';
 const root=document.documentElement;
 const contrast=matchMedia('(prefers-contrast:more)');
 const opaque=matchMedia('(prefers-reduced-transparency:reduce)');
 const apply=()=>{root.dataset.glass=contrast.matches||opaque.matches?'solid':'frosted';};
 contrast.addEventListener('change',apply);
 opaque.addEventListener('change',apply);
 apply();
})();
