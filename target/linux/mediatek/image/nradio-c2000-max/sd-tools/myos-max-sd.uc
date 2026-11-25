'use strict';
import { popen } from 'fs';
function run(action) {
 let pipe=popen('/usr/libexec/myos-max-boot '+action+' 2>/dev/null','r');
 if (!pipe) return { ok:false,error:'helper_unavailable' };
 let output=pipe.read('all'), status=pipe.close();
 try {
  let result=json(output);
  if(type(result)=='object') return result;
 } catch(e) {}
 return { ok:false,error:'helper_failed',status };
}

function read_nros_version() {
 let pipe = popen("strings /dev/mtd2ro 2>/dev/null | grep -m1 '^kp_soft_ver='", 'r');
 if (!pipe) return null;
 let field = trim(pipe.read('all') || '');
 pipe.close();
 let value = match(field, /^kp_soft_ver=([1-9][0-9]{8,9})$/);
 return value ? int(value[1]) : null;
}

return {
 'myos.max.sd': {
  status: {
   call:function() {
    let result = run('status');
    let version = read_nros_version();
    if (version != null) {
     // NROS packs major, minor and build as %d%03d%05d.
     result.nros_version = sprintf('%d.%d.%d',
      int(version / 100000000), int(version / 100000) % 1000, version % 100000);
     result.nros_update_needed = version < 200300009;
    }
    return result;
   }
  },
  stock: {
   args: { confirmed:false },
   call:function(req) {
    if(req.args.confirmed!==true) return {ok:false,error:'confirmation_required'};
    return run('stock');
   }
  }
 }
};

