const fs = require("fs");

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
  } catch (err) {
    return [LCC_Result, 1, {
      __proto__: LCC_IOError,
      message: err.toString(),
    }];
  }
}
