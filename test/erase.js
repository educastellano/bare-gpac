const test = require('brittle')
const { ISOFile } = require('bare-gpac')
const fixture = require('./helpers/fixture')

test('eraseItemData() erases a single idat extent', (t) => {
  const input = fixture.make({ idat: true })
  const expected = Buffer.concat([fixture.image, Buffer.alloc(fixture.exif.length), fixture.xmp])
  using file = new ISOFile(input)

  file.eraseItemData(7)
  const erased = file.readItem(7)
  const output = file.write()

  t.alike(erased, Buffer.alloc(fixture.exif.length))
  t.alike(fixture.idatBytes(output), expected)
})

test('eraseItemData() can be followed by removal', (t) => {
  const input = fixture.make({ idat: true })
  const expected = Buffer.concat([fixture.image, Buffer.alloc(fixture.exif.length), fixture.xmp])
  using file = new ISOFile(input)

  file.eraseItemData(7)
  file.removeItem(7)
  const output = file.write()

  using next = new ISOFile(output)
  t.absent(next.items().some((item) => item.id === 7))
  t.alike(fixture.idatBytes(output), expected)
})

test('eraseItemData() preserves gaps between extents', (t) => {
  const input = fixture.make({ idat: true, split: true })
  const location = fixture.locationOffset(input, 1)
  input.writeUInt32BE(fixture.image.length + fixture.exif.length - 5, location + 16)
  input.writeUInt32BE(5, location + 20)
  const expected = Buffer.concat([
    fixture.image,
    Buffer.alloc(5),
    fixture.exif.subarray(5, -5),
    Buffer.alloc(5),
    fixture.xmp
  ])
  using file = new ISOFile(input)

  file.eraseItemData(7)

  t.alike(fixture.idatBytes(file.write()), expected)
})

test('eraseItemData() rejects overlaps without editing any bytes or items', (t) => {
  for (const absolute of [false, true]) {
    const input = fixture.make({ idat: true, split: true })
    const other = fixture.locationOffset(input, 2)
    const base = absolute ? fixture.metaBox(input, 'idat').start : 0
    // Only the target's second extent overlaps, after the first passed validation.
    const offset = fixture.image.length + 5
    if (absolute) input.writeUInt16BE(0, other + 2)
    input.writeUInt32BE(base + offset, other + 8)
    input.writeUInt32BE(1, other + 12)
    input.writeUInt32BE(base + input.readUInt32BE(other + 16), other + 16)
    using file = new ISOFile(input)
    const items = file.items()

    t.exception.all(() => file.eraseItemData(7), { code: 'ERR_GPAC' })

    t.alike(file.items(), items)
    t.alike(fixture.idatBytes(file.write()), fixture.idatBytes(input))
  }
})

test('eraseItemData() validates every range before changing anything', (t) => {
  for (const index of [1, 2]) {
    for (const field of [16, 20]) {
      const input = fixture.make({ idat: true, split: true })
      const location = fixture.locationOffset(input, index)
      const id = input.readUInt16BE(location)
      input.writeUInt32BE(0xffffffff, location + field)
      using file = new ISOFile(input)

      t.exception.all(() => file.eraseItemData(7), { code: 'ERR_GPAC' })
      // Remove the malformed item so GPAC can write the unchanged idat bytes.
      file.removeItem(id)

      t.alike(fixture.idatBytes(file.write()), fixture.idatBytes(input))
    }
  }
})

test('eraseItemData() handles a single implicit extent', (t) => {
  const input = fixture.make({ idat: true })
  input.writeUInt32BE(0, fixture.locationOffset(input, 2) + 12)
  const expected = Buffer.concat([fixture.image, fixture.exif, Buffer.alloc(fixture.xmp.length)])
  using file = new ISOFile(input)

  file.eraseItemData(12)

  t.alike(fixture.idatBytes(file.write()), expected)
})

test('eraseItemData() rejects ambiguous extents', (t) => {
  for (const index of [1, 2]) {
    const input = fixture.make({ idat: true, split: true })
    input.writeUInt32BE(0, fixture.locationOffset(input, index) + 20)
    using file = new ISOFile(input)

    t.exception.all(() => file.eraseItemData(7), { code: 'ERR_GPAC' })

    t.alike(fixture.idatBytes(file.write()), fixture.idatBytes(input))
  }
})

test('eraseItemData() rejects unresolved storage', (t) => {
  for (const index of [1, 2]) {
    const input = fixture.make({ idat: true })
    input.writeUInt16BE(2, fixture.locationOffset(input, index) + 2)
    using file = new ISOFile(input)

    t.exception.all(() => file.eraseItemData(7), { code: 'ERR_GPAC' })
    file.removeItem(index === 1 ? 7 : 12)

    t.alike(fixture.idatBytes(file.write()), fixture.idatBytes(input))
  }
})

test('eraseItemData() rejects the primary image and missing items', (t) => {
  const input = fixture.make({ idat: true })
  using file = new ISOFile(input)

  t.exception.all(() => file.eraseItemData(1), { code: 'ERR_GPAC' })
  t.exception.all(() => file.eraseItemData(99), { code: 'ERR_GPAC' })

  t.alike(fixture.idatBytes(file.write()), fixture.idatBytes(input))
})

test('eraseItemData() leaves mdat and newly added items alone', (t) => {
  using file = new ISOFile(fixture.make())
  const data = Buffer.from('new metadata')
  const added = file.addItem(data, { type: 'Exif' })

  file.eraseItemData(added)
  file.eraseItemData(7)

  t.alike(file.readItem(added), data)
  t.alike(file.readItem(7), fixture.exif)
})

test('eraseItemData() keeps the item and references', (t) => {
  const input = fixture.make({ idat: true })
  using file = new ISOFile(input)
  const items = file.items()

  file.eraseItemData(7)
  const output = file.write()

  using next = new ISOFile(output)
  t.alike(fixture.references(output), fixture.references(input))
  t.alike(next.items(), items)
})

test('eraseItemData() rejects closed files', (t) => {
  using file = new ISOFile(fixture.make({ idat: true }))

  file.destroy()

  t.exception.all(() => file.eraseItemData(7))
})
