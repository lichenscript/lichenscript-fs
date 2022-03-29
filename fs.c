#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
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

typedef struct FSMappedFile {
  int64_t size;
  uintptr_t mapped_mem;
  int fd;
} FSMappedFile;

static int lc_open_readable_mapped_mem(const char* path, FSMappedFile* mapped_file) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    return fd;
  }

  struct stat st;
  int ec = fstat(fd, &st);
  if (ec < 0) {
    return ec;
  }

  void* mapped_mem = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);;
  if (mapped_mem == MAP_FAILED) {
    return -1;
  }

  mapped_file->fd = fd;
  mapped_file->mapped_mem = (uintptr_t)mapped_mem;
  mapped_file->size = st.st_size;

  return 0;
}

static void lc_close_readable_mapped_mem(FSMappedFile* mapped_file) {
  munmap((void*)mapped_file->mapped_mem, mapped_file->size);
  mapped_file->mapped_mem = 0;
  close(mapped_file->fd);
}

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

LCValue lc_fs_read_file_content(LCRuntime* rt, LCValue this, int argc, LCValue* args) {
  LCValue result, err;
  FSMappedFile mapped_file;
  const char* u8str = LCToUTF8(rt, args[0]);

  int ec = lc_open_readable_mapped_mem(u8str, &mapped_file);
  if (ec < 0) {
    goto fail;
  }

  LCValue str = LCNewStringFromCStringLen(rt, (const unsigned char*)mapped_file.mapped_mem, mapped_file.size);

  LCFreeUTF8(rt, u8str);

  result =  LCNewUnionObject(rt, LC_STD_CLS_ID_RESULT, 0, 1, (LCValue[]) { str });
  LCRelease(rt, str);

  lc_close_readable_mapped_mem(&mapped_file);

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

LCValue lc_fs_read_file(LCRuntime* rt, LCValue this, int argc, LCValue* args) {
  LCValue err;
  FSMappedFile mapped_file;
  LCValue result;
  const char* u8str = LCToUTF8(rt, args[0]);

  int ec = lc_open_readable_mapped_mem(u8str, &mapped_file);
  if (ec < 0) {
    goto fail;
  }

  int64_t new_cap = 2;

  while (new_cap < mapped_file.size) {
    new_cap *= 2;
  }

  LCBuffer* buffer = lc_std_new_buffer_with_cap(rt, new_cap);
  buffer->length = mapped_file.size;
  memcpy(buffer->data, (void*)mapped_file.mapped_mem, mapped_file.size);

  LCValue buffer_val = MK_PTR(buffer, LC_TY_CLASS_OBJECT);

  result =  LCNewUnionObject(rt, LC_STD_CLS_ID_RESULT, 0, 1, (LCValue[]) { buffer_val });
  LCRelease(rt, buffer_val);
  lc_close_readable_mapped_mem(&mapped_file);

  return result;
fail:
  err = LCC_IOError_init(rt);

  LCCast(err, FSIOError*)->code = errno;

  result = LCNewUnionObject(rt, LC_STD_CLS_ID_RESULT, 1, 1, (LCValue[]) { err });

  LCRelease(rt, err);
  LCFreeUTF8(rt, u8str);

  return result;
}

LCValue lc_fs_write_file(LCRuntime* rt, LCValue this, int argc, LCValue* args) {
  return LC_NULL;
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
