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
  unsigned char* mapped_mem;
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

  void* mapped_mem = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if (mapped_mem == MAP_FAILED) {
    return -1;
  }

  mapped_file->fd = fd;
  mapped_file->mapped_mem = (unsigned char*)mapped_mem;
  mapped_file->size = st.st_size;

  return 0;
}

static int lc_open_writable_mapped_mem(const char* path, FSMappedFile* mapped_file) {
  int fd = open(path, O_RDWR | O_CREAT, 0644);
  if (fd < 0) {
    return fd;
  }

  struct stat st;
  int ec = fstat(fd, &st);
  if (ec < 0) {
    return ec;
  }

  void* mapped_mem = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapped_mem == MAP_FAILED) {
    return -1;
  }

  mapped_file->fd = fd;
  mapped_file->mapped_mem = (unsigned char*)mapped_mem;
  mapped_file->size = st.st_size;

  return 0;
}

static int lc_resize_writable_mapped_mem(const char* path, FSMappedFile* mapped_file, size_t size) {
  munmap(mapped_file->mapped_mem, mapped_file->size);
  mapped_file->mapped_mem = NULL;

  int fd = mapped_file->fd;

  int ec = ftruncate(fd, size);
  if (ec < 0) {
    return ec;
  }

  void* mapped_mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapped_mem == MAP_FAILED) {
    return -1;
  }

  mapped_file->mapped_mem = (unsigned char*)mapped_mem;
  mapped_file->size = size;

  return 0;
}

static void lc_close_mapped_mem(FSMappedFile* mapped_file) {
  if (mapped_file->mapped_mem != NULL) {
    munmap((void*)mapped_file->mapped_mem, mapped_file->size);
    mapped_file->mapped_mem = NULL;
  }
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

  lc_close_mapped_mem(&mapped_file);

  return result;
fail:
  err = LCC_IOError_init(rt);

  LCCast(err, FSIOError*)->code = errno;

  result = LCNewUnionObject(rt, LC_STD_CLS_ID_RESULT, 1, 1, (LCValue[]) { err });

  LCRelease(rt, err);
  LCFreeUTF8(rt, u8str);

  return result;
}

LCValue lc_fs_write_file_content(LCRuntime* rt, LCValue this, int argc, LCValue* args) {
  LCValue err;
  size_t content_len;
  const char* u8str = LCToUTF8(rt, args[0]);
  FSMappedFile mapped_file;

  int ec = lc_open_writable_mapped_mem(u8str, &mapped_file);
  if (ec < 0) {
    goto fail;
  }

  const char* u8content = LCToUTF8Len(rt, &content_len, args[1]);

  memcpy(mapped_file.mapped_mem, u8content, content_len);

  LCFreeUTF8(rt, u8content);

  lc_close_mapped_mem(&mapped_file);

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
  memcpy(buffer->data, mapped_file.mapped_mem, mapped_file.size);

  LCValue buffer_val = MK_PTR(buffer, LC_TY_CLASS_OBJECT);

  result =  LCNewUnionObject(rt, LC_STD_CLS_ID_RESULT, 0, 1, (LCValue[]) { buffer_val });
  LCRelease(rt, buffer_val);
  lc_close_mapped_mem(&mapped_file);

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
  LCValue err;
  const char* u8str = LCToUTF8(rt, args[0]);
  FSMappedFile mapped_file;

  int ec = lc_open_writable_mapped_mem(u8str, &mapped_file);
  if (ec < 0) {
    goto fail;
  }

  LCBuffer* buffer = (LCBuffer*)args[1].ptr_val;

  memcpy(mapped_file.mapped_mem, buffer->data, buffer->length);

  lc_close_mapped_mem(&mapped_file);

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
