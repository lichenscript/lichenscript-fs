
#ifndef LC_FS
#define LC_FS

#include "runtime.h"

void lc_fs_init(LCRuntime* rt);

LCValue lc_fs_read_file_content(LCRuntime* rt, LCValue this, int argc, LCValue* args);

LCValue lc_fs_write_file_content(LCRuntime* rt, LCValue this, int argc, LCValue* args);

LCValue lc_fs_unlink(LCRuntime* rt, LCValue this, int argc, LCValue* args);

#endif
