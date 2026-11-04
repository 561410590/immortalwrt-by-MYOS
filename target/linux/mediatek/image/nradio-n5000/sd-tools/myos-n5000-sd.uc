'use strict';
import { popen } from 'fs';
function run(action) {
 let pipe=popen('/usr/libexec/myos-n5000-boot '+action+' 2>/dev/null','r');
 if (!pipe) return { ok:false,error:'helper_unavailable' };
 let output=pipe.read('all'), status=pipe.close();
 try {
  let result=json(output);
  if(type(result)=='object') return result;
 } catch(e) {}
 return { ok:false,error:'helper_failed',status };
}
return {
 'myos.ultra.sd': {
  status: { call:function() { return run('status'); } },
  stock: {
   args: { confirmed:false },
   call:function(req) {
    if(req.args.confirmed!==true) return {ok:false,error:'confirmation_required'};
    return run('stock');
   }
  }
 }
};


