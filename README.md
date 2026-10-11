# bare-gpac

GPAC bindings for Bare.

```sh
npm install bare-gpac
```

## Usage

```js
const gpac = require('bare-gpac')

const image = require('./sample.heic', { with: { type: 'binary' } })

using file = new gpac.ISOFile(image)

for (const item of file.items()) {
  if (item.type === 'Exif') {
    file.removeItem(item.id)
  }
}

const output = file.write()
```

## API

> [!IMPORTANT]
> Only the isomedia API is currently supported.

### ISOFile

| Method                 | Description                                                                                  |
| ---------------------- | -------------------------------------------------------------------------------------------- |
| `new ISOFile(data)`    | Open a Buffer or Uint8Array for editing.                                                     |
| `items()`              | List root metadata items: `id`, `type`, `primary`, `name`, `contentType`, `contentEncoding`. |
| `readItem(id)`         | Return the item's bytes as a Buffer.                                                         |
| `addItem(data, opts)`  | Copy item data and return its assigned ID.                                                   |
| `removeItem(id, opts)` | Remove an item. Set `{ keepRefs: true }` to retain its references; defaults to `false`.      |
| `eraseItemData(id)`    | Zero the item's `idat` bytes without removing it.                                            |
| `findBox(type)`        | Find the first matching top-level box; returns a `Box` with `id` and `type`, or `null`.      |
| `removeBox(id)`        | Remove an unknown top-level box, such as `sefd`. Use an ID from this file's `findBox()`.     |
| `write()`              | Return the rewritten file and close it.                                                      |
| `destroy()`            | Discard changes and release resources.                                                       |

> [!NOTE]
> `eraseItemData()` is an extra utility provided by this library, not part of GPAC. Call it before `removeItem()` to clear an item's `idat` bytes (`mdat` needs no erasure as `write()` copies only referenced data).

## License

Apache-2.0
