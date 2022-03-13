const fs = require("fs");

const LCC_IOError = {
  __proto__: LCC_Object,
  [clsNameSym]: "IOError",
  toString: IOError_toString,
}

function IOError_toString() {
  return this.message;
}

function lc_fs_read_file_content(path) {
  try {
    const data = fs.readFileSync(path, 'utf8');
    return [LCC_Result, 0, data];
  } catch (err) {
    return [LCC_Result, 1, {
      __proto__: LCC_IOError,
      message: err.toString(),
    }];
  }
}

function lc_fs_write_file_content(path, content) {
  try {
    fs.writeFileSync(path, content);
    return [LCC_Result, 0];
  } catch (err) {
    return [LCC_Result, 1, {
      __proto__: LCC_IOError,
      message: err.toString(),
    }];
  }
}

function lc_fs_unlink(path) {
  try {
    fs.unlinkSync(path);
    return [LCC_Result, 0];
  } catch (err) {
    return [LCC_Result, 1, {
      __proto__: LCC_IOError,
      message: err.toString(),
    }];
  }
}
