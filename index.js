const binding = require('./binding')

class ISOFile {
  constructor(data) {
    if (!(data instanceof Uint8Array) || !(data.buffer instanceof ArrayBuffer)) {
      throw new TypeError('Expected a Buffer or Uint8Array')
    }

    binding.init(this, data.buffer, data.byteOffset, data.byteLength)
  }

  items() {
    return binding.items(this)
  }

  readItem(id) {
    return Buffer.from(binding.readItem(this, id))
  }

  addItem(data, opts = {}) {
    const { type, id = 0, name = '', contentType = '', contentEncoding = '' } = opts

    return binding.addItem(
      this,
      data.buffer,
      data.byteOffset,
      data.byteLength,
      id,
      type,
      name,
      contentType,
      contentEncoding
    )
  }

  removeItem(id, opts = {}) {
    const { keepRefs = false } = opts

    binding.removeItem(this, id, keepRefs)
  }

  eraseItemData(id) {
    binding.eraseItemData(this, id)
  }

  findBox(type) {
    const box = binding.findBox(this, type)
    return box ? new Box(box.id, box.type) : null
  }

  removeBox(id) {
    binding.removeBox(this, id)
  }

  write() {
    try {
      return Buffer.from(binding.write(this))
    } finally {
      this.destroy()
    }
  }

  destroy() {
    binding.destroy(this)
  }

  [Symbol.dispose]() {
    this.destroy()
  }
}

class Box {
  constructor(id, type) {
    this.id = id
    this.type = type
  }
}

exports.ISOFile = ISOFile
exports.Box = Box
