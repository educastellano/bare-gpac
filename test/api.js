const test = require('brittle')
const { ISOFile, Box } = require('..')
const fixture = require('./helpers/fixture')

test('constructor opens a valid buffer', (t) => {
  const input = require('./fixtures/sample.heic', {
    with: { type: 'binary' }
  })

  using file = new ISOFile(input)

  t.is(file.items().find((item) => item.primary).type, 'hvc1')
})

test('constructor rejects invalid input', (t) => {
  t.exception.all(() => new ISOFile(123))
})

test('constructor copies the input buffer slice', (t) => {
  const input = require('./fixtures/sample.heic', {
    with: { type: 'binary' }
  })
  const buffer = Buffer.concat([Buffer.from('!'), input, Buffer.from('!')])
  const slice = buffer.subarray(1, -1)

  using file = new ISOFile(slice)
  const image = file.readItem(1)
  buffer.fill(0)

  t.alike(file.readItem(1), image)
})

test('items() - heic', (t) => {
  const input = require('./fixtures/sample.heic', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(input)

  const items = file.items()

  t.is(items.find((item) => item.primary).type, 'hvc1')
})

test('items() - avif', (t) => {
  const input = require('./fixtures/sample.avif', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(input)

  const items = file.items()

  t.is(items.find((item) => item.primary).type, 'av01')
})

test('readItem() - heic', (t) => {
  const input = require('./fixtures/sample.heic', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(input)

  const data = file.readItem(1)

  t.ok(Buffer.isBuffer(data))
  t.ok(data.length > 0)
})

test('readItem() - avif', (t) => {
  const input = require('./fixtures/sample.avif', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(input)

  const data = file.readItem(1)

  t.ok(Buffer.isBuffer(data))
  t.ok(data.length > 0)
})

test('readItem() returns own buffer', (t) => {
  const input = require('./fixtures/sample.heic', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(input)
  const original = Buffer.from(file.readItem(1))

  const data = file.readItem(1)
  data.fill(0)

  t.alike(file.readItem(1), original)
})

test('readItem() returns newly added items before writing', (t) => {
  const input = require('./fixtures/sample.heic', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(input)
  const expected = Buffer.from('new item data')
  const id = file.addItem(Buffer.from(expected), { type: 'mime' })

  const item = file.readItem(id)
  const original = Buffer.from(item)
  item.fill(0)

  t.alike(original, expected)
  t.alike(file.readItem(id), expected)
})

test('readItem() missing item IDs propagate GPAC errors', (t) => {
  const input = require('./fixtures/sample.heic', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(input)

  t.exception.all(() => file.readItem(99), { code: 'ERR_GPAC' })
})

test('removeItem()', (t) => {
  const input = require('./fixtures/sample.heic', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(input)
  const exif = file.items().find((item) => item.type === 'Exif')
  const remaining = file.items().filter((item) => item.id !== exif.id)
  const data = remaining.map((item) => file.readItem(item.id))

  file.removeItem(exif.id)

  using next = new ISOFile(file.write())
  t.alike(next.items(), remaining)
  t.alike(
    remaining.map((item) => next.readItem(item.id)),
    data
  )
})

test('removeItem() the primary image cannot be removed', (t) => {
  const input = require('./fixtures/sample.heic', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(input)
  const primary = file.items().find((item) => item.primary)
  const image = file.readItem(primary.id)

  t.exception.all(() => file.removeItem(primary.id), { code: 'ERR_GPAC' })

  t.alike(file.readItem(primary.id), image)
})

test('removeItem can preserve references', (t) => {
  const input = fixture.make()
  using file = new ISOFile(input)

  file.removeItem(7, { keepRefs: true })
  const output = file.write()

  t.alike(fixture.references(output), fixture.references(input))
})

test('removeItem() cleans up references with the GPAC patch', (t) => {
  using file = new ISOFile(fixture.make())

  file.removeItem(7)
  file.removeItem(12)
  const output = file.write()

  const refs = fixture.references(output)
  t.is(fixture.boxes(refs, 4).length, 0, 'no dangling references')
})

test('removeItem() missing item IDs propagate GPAC errors', (t) => {
  const input = require('./fixtures/sample.heic', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(input)

  t.exception.all(() => file.removeItem(99), { code: 'ERR_GPAC' })
})

test('addItem()', (t) => {
  const input = require('./fixtures/sample.avif', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(input)
  const data = Buffer.from('<x:xmpmeta>author</x:xmpmeta>')
  const options = {
    id: 42,
    type: 'mime',
    name: 'XMP',
    contentType: 'application/rdf+xml',
    contentEncoding: 'identity'
  }

  const id = file.addItem(data, options)

  using next = new ISOFile(file.write())
  t.is(id, options.id)
  t.alike(
    next.items().find((item) => item.id === id),
    { ...options, primary: false }
  )
  t.alike(next.readItem(id), data)
})

test('addItem() rejects empty data', (t) => {
  const input = require('./fixtures/sample.heic', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(input)
  const empty = Buffer.alloc(0)

  t.exception.all(() => file.addItem(empty, { type: 'Exif' }))
})

test('addItem() rejects short types', (t) => {
  const input = require('./fixtures/sample.heic', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(input)
  const data = Buffer.from('metadata')

  t.exception.all(() => file.addItem(data, { type: 'abc' }), RangeError)
})

test('findBox()', (t) => {
  const input = require('./fixtures/sample.avif', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(Buffer.concat([input, fixture.box('sefd', Buffer.from('vendor data'))]))

  const box1 = file.findBox('sefd')
  const box2 = file.findBox('ftyp')
  const missing = file.findBox('none')

  t.ok(box1 instanceof Box)
  t.is(box1.type, 'sefd')
  t.is(box2.type, 'ftyp')
  t.is(missing, null)
})

test('findBox() rejects short types', (t) => {
  const input = require('./fixtures/sample.heic', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(input)

  t.exception.all(() => file.findBox('abc'), RangeError)
})

test('findBox() rejects long types', (t) => {
  const input = require('./fixtures/sample.heic', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(input)

  t.exception.all(() => file.findBox('abcde'), RangeError)
})

test('findBox() is top-level for now', (t) => {
  const input = require('./fixtures/sample.heic', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(input)

  const box = file.findBox('iloc')

  t.is(box, null)
})

test('findBox() repeated lookups return the same ID', (t) => {
  const input = require('./fixtures/sample.heic', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(input)

  const first = file.findBox('meta')
  const second = file.findBox('meta')

  t.is(second.id, first.id)
})

test('findBox() removed box IDs are not reused', (t) => {
  const sample = require('./fixtures/sample.avif', {
    with: { type: 'binary' }
  })
  const input = Buffer.concat([
    sample,
    fixture.box('sefd', Buffer.from('first vendor data')),
    fixture.box('sefd', Buffer.from('second vendor data'))
  ])
  using file = new ISOFile(input)
  const first = file.findBox('sefd')

  file.removeBox(first.id)
  const second = file.findBox('sefd')

  t.not(second.id, first.id)
})

test('removeBox()', (t) => {
  const input = require('./fixtures/sample.avif', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(Buffer.concat([input, fixture.box('sefd', Buffer.from('vendor data'))]))
  const box = file.findBox('sefd')

  file.removeBox(box.id)

  using next = new ISOFile(file.write())
  t.is(next.findBox('sefd'), null)
})

test('removeBox() rejects structural boxes', (t) => {
  const input = require('./fixtures/sample.heic', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(input)
  const box = file.findBox('meta')

  t.exception.all(() => file.removeBox(box.id), { code: 'ERR_GPAC' })

  t.is(file.findBox('meta').id, box.id)
})

test('removeBox() rejects invalid IDs', (t) => {
  const input = require('./fixtures/sample.heic', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(input)

  t.exception.all(() => file.removeBox(0))
})

test('removeBox() rejects removed IDs', (t) => {
  const input = require('./fixtures/sample.avif', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(Buffer.concat([input, fixture.box('sefd', Buffer.from('vendor data'))]))
  const box = file.findBox('sefd')

  file.removeBox(box.id)

  t.exception.all(() => file.removeBox(box.id))
})

test('write() - heic', (t) => {
  const input = require('./fixtures/sample.heic', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(input)
  const items = file.items()
  const image = file.readItem(1)

  const output = file.write()

  using next = new ISOFile(output)
  t.alike(next.items(), items)
  t.alike(next.readItem(1), image)
})

test('write() - avif', (t) => {
  const input = require('./fixtures/sample.avif', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(input)
  const items = file.items()
  const image = file.readItem(1)

  const output = file.write()

  using next = new ISOFile(output)
  t.alike(next.items(), items)
  t.alike(next.readItem(1), image)
})

test('write() preserves data across multiple output chunks', (t) => {
  const image = Buffer.alloc(200000, 0x5a)
  const input = fixture.make({ image })
  using file = new ISOFile(input)

  const output = file.write()
  using next = new ISOFile(output)

  t.alike(next.readItem(1), image)
})

test('write() closes the file', (t) => {
  const input = require('./fixtures/sample.heic', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(input)

  file.write()

  t.exception.all(() => file.findBox('meta'))
})

test('destroy() closes the file', (t) => {
  const input = require('./fixtures/sample.heic', {
    with: { type: 'binary' }
  })
  using file = new ISOFile(input)

  file.destroy()

  t.exception.all(() => file.items())
})
