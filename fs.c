#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/errno.h>
#include <string.h>
#include "fs.h"
#include "runtime.h"

static LCClassID FS_IOError_class_id;

#define LC_FS_BLOCK_SIZE (8 * 1024)

typedef struct FSIOError {
  LCGCObjectHeader header;
  int code;
} FSIOError;

static LCClassDef FSIOErrorDef = {
  "IOError",
  NULL,
  NULL,
};

#define MK_PTR(v, ty) (LCValue){ { .ptr_val = (LCObject*)v }, ty }

LCValue FSIOError_toString(LCRuntime* rt, LCValue this, int arg_len, LCValue* args) {
  FSIOError* ptr = (FSIOError*)this.ptr_val;
  const char* error_str = strerror(ptr->code);
  return LCNewStringFromCString(rt, (const unsigned char*)error_str);
}

static LCClassMethodDef FSIOErrorMethods[] = {
  { "toString", 0, FSIOError_toString },
};

LCValue LCC_IOError_init(LCRuntime* rt) {
  FSIOError* ptr = lc_malloc(rt, sizeof(FSIOError));
  lc_init_object(rt, FS_IOError_class_id, (LCGCObject*)ptr);
  ptr->code = 0;
  return MK_PTR(ptr, LC_TY_CLASS_OBJECT);
}

void lc_fs_init(LCRuntime* rt) {
  FS_IOError_class_id = LCDefineClass(rt, 0, &FSIOErrorDef);
  LCDefineClassMethod(rt, FS_IOError_class_id, FSIOErrorMethods, countof(FSIOErrorMethods));
}

static char* lc_fs_read_file_into_buffer(int fd, int* size) {
  int cap = (LC_FS_BLOCK_SIZE) * 2;
  char* buffer = (char*)malloc(cap);
  char* p = buffer;
  ssize_t read_bytes;

  while (1) {
    read_bytes = read(fd, p, LC_FS_BLOCK_SIZE);
    if (read_bytes == 0) {
      break;
    } else if (read_bytes < 0) {
      if (size) {
        *size = read_bytes;
      }
      free(buffer);
      return NULL;
    }

    p += read_bytes;

    ssize_t s = (p - buffer);
    if ((cap - s) < LC_FS_BLOCK_SIZE) {
      cap += LC_FS_BLOCK_SIZE;
      buffer = (char*)realloc(buffer, cap);
      p = buffer + s;
    }
  }

  if (size) {
    *size = p - buffer;
  }
  return buffer;
}

LCValue lc_fs_read_file_content(LCRuntime* rt, LCValue this, int argc, LCValue* args) {
  LCValue result, err;
  char *buffer;
  const char* u8str = LCToUTF8(rt, args[0]);
  int size = 0;
  int fd = open(u8str, O_RDONLY);

  if (fd < 0) {
    goto fail;
  }

  buffer = lc_fs_read_file_into_buffer(fd, &size);

  close(fd);

  if (size < 0) {
    free(buffer);
    goto fail;
  }

  LCValue str = LCNewStringFromCStringLen(rt, (const unsigned char*)buffer, size);

  LCFreeUTF8(rt, u8str);

  free(buffer);

  result =  LCNewUnionObject(rt, LC_STD_CLS_ID_RESULT, 0, 1, (LCValue[]) { str });
  LCRelease(rt, str);
  return result;
fail:
  err = LCC_IOError_init(rt);

  LCCast(err, FSIOError*)->code = errno;

  result = LCNewUnionObject(rt, LC_STD_CLS_ID_RESULT, 1, 1, (LCValue[]) { err });

  LCRelease(rt, err);
  LCFreeUTF8(rt, u8str);

  return result;
}

static int lc_write_file_content(int fd, const char* content, size_t content_len) {
  int ec;
  size_t len = content_len;
  const char* p = content;

  while (1) {
    size_t written_len = len < LC_FS_BLOCK_SIZE ? len : LC_FS_BLOCK_SIZE;
    ec = write(fd, p, written_len);
    if (ec == 0) {
      break;
    } else if (ec < 0) {
      return ec;
    }
    p += ec;
    len -= ec;
  }

  return 0;
}

LCValue lc_fs_write_file_content(LCRuntime* rt, LCValue this, int argc, LCValue* args) {
  LCValue err;
  size_t content_len;
  const char* u8str = LCToUTF8(rt, args[0]);
  int ec;
  int size = 0;
  int fd = open(u8str, O_WRONLY | O_CREAT, 0666);

  if (fd < 0) {
    goto fail;
  }

  const char* u8content = LCToUTF8Len(rt, &content_len, args[1]);

  ec = lc_write_file_content(fd, u8content, content_len);
  if (ec < 0) {
    LCFreeUTF8(rt, u8content);
    close(fd);
    goto fail;
  }

  LCFreeUTF8(rt, u8content);
  close(fd);
  LCFreeUTF8(rt, u8str);

  return LCNewUnionObject(rt, LC_STD_CLS_ID_RESULT, 0, 1, (LCValue[]) { LC_NULL });
fail:
  err = LCC_IOError_init(rt);

  LCCast(err, FSIOError*)->code = errno;

  LCValue result = LCNewUnionObject(rt, LC_STD_CLS_ID_RESULT, 1, 1, (LCValue[]) { err });

  LCRelease(rt, err);
  LCFreeUTF8(rt, u8str);

  return result;
}

LCValue lc_fs_unlink(LCRuntime* rt, LCValue this, int argc, LCValue* args) {
  LCValue err;
  const char* u8str = LCToUTF8(rt, args[0]);

  int ec = unlink(u8str);

  LCFreeUTF8(rt, u8str);

  if (ec < 0) {
    goto fail;
  }

  return LCNewUnionObject(rt, LC_STD_CLS_ID_RESULT, 0, 1, (LCValue[]) { LC_NULL });
fail:
  err = LCC_IOError_init(rt);

  LCCast(err, FSIOError*)->code = errno;

  LCValue result = LCNewUnionObject(rt, LC_STD_CLS_ID_RESULT, 1, 1, (LCValue[]) { err });
  LCRelease(rt, err);

  return result;
}
