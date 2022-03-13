
# Filesystem for LichenScript

## Install

```
npm install --save lichenscript-fs
```

## Usage

Read data from file:

```js
import fs from "lichenscript-fs";

function main() {
  const content = fs.readFileContent("/tmp/foo.txt").unwrap();
  print(content);
}
```

Write data to file:

```js
import fs from "lichenscript-fs";

function main() {
  fs.writeFileContent("foo.txt", "Hello World").unwrap();
}
```

Unlink a file:

```js
import fs from "lichenscript-fs";

function main() {
  fs.unlink("foo.txt").unwrap();
}
```
