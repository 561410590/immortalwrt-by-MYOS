SECTIONS
{
    .sub_mod_initcall.init : {
        __sub_mod_initcall_start = .;
        KEEP(*(.sub_mod_initcall.init))
        __sub_mod_initcall_end = .;
    }
    .sub_mod_exitcall.exit : {
        __sub_mod_exitcall_start = .;
        KEEP(*(.sub_mod_exitcall.exit))
        __sub_mod_exitcall_end = .;
    }
}
