exports.image = Buffer.from('image-payload-that-must-survive')
exports.exif = Buffer.from('private-exif-location-59.91-10.75')
exports.xmp = Buffer.from('<x:xmpmeta>private-xmp-author</x:xmpmeta>')

exports.make = function make({ idat = false, split = false, image = exports.image } = {}) {
  const ftyp = box('ftyp', Buffer.from('mif1'), u32(0), Buffer.from('mif1heic'))
  const items = [
    { id: 1, type: 'grid', data: image },
    { id: 7, type: 'Exif', data: exports.exif },
    { id: 12, type: 'mime', contentType: 'application/rdf+xml', data: exports.xmp }
  ]
  const payload = Buffer.concat(items.map((item) => item.data))
  const hdlr = full('hdlr', 0, u32(0), Buffer.from('pict'), Buffer.alloc(12), Buffer.from('test\0'))
  const pitm = full('pitm', 0, u16(1))
  const iinf = full(
    'iinf',
    0,
    u16(items.length),
    ...items.map((item) =>
      full(
        'infe',
        2,
        u16(item.id),
        u16(0),
        Buffer.from(item.type),
        Buffer.from('item\0'),
        item.type === 'mime' ? Buffer.from(`${item.contentType}\0\0`) : Buffer.alloc(0)
      )
    )
  )
  const iref = full(
    'iref',
    0,
    box('cdsc', u16(7), u16(1), u16(1)),
    box('cdsc', u16(12), u16(1), u16(1)),
    box('test', u16(1), u16(2), u16(7), u16(12))
  )
  const iprp = box(
    'iprp',
    box('ipco', full('ispe', 0, u32(2), u32(2)), box('irot', Buffer.from([1]))),
    full('ipma', 0, u32(1), u16(1), Buffer.from([2, 0x81, 2]))
  )

  function meta(base) {
    let offset = base
    const locations = []

    for (const item of items) {
      const extentCount = split ? 2 : 1
      const firstExtentLength = split ? 5 : item.data.length
      const extents = [u32(offset), u32(firstExtentLength)]

      if (split) {
        extents.push(u32(offset + firstExtentLength), u32(item.data.length - firstExtentLength))
      }

      offset += item.data.length
      locations.push(
        Buffer.concat([u16(item.id), u16(idat ? 1 : 0), u16(0), u16(extentCount), ...extents])
      )
    }

    const iloc = full('iloc', 1, Buffer.from([0x44, 0]), u16(items.length), ...locations)

    return full(
      'meta',
      0,
      hdlr,
      pitm,
      iinf,
      iloc,
      iref,
      iprp,
      ...(idat ? [box('idat', payload)] : [])
    )
  }

  const header = meta(0)
  return idat
    ? Buffer.concat([ftyp, header])
    : Buffer.concat([ftyp, meta(ftyp.length + header.length + 8), box('mdat', payload)])
}

exports.boxes = function boxes(data, start = 0, end = data.length) {
  const result = []
  while (start < end) {
    const size = data.readUInt32BE(start)
    if (size < 8 || start + size > end) throw new Error('Invalid box size')
    result.push({
      type: data.toString('ascii', start + 4, start + 8),
      start: start + 8,
      end: start + size
    })
    start += size
  }
  return result
}

exports.box = box

exports.references = function references(data) {
  const refs = exports.metaBox(data, 'iref')
  return refs ? data.subarray(refs.start, refs.end) : null
}

exports.metaBox = function metaBox(data, type) {
  const meta = exports.boxes(data).find((box) => box.type === 'meta')
  return exports.boxes(data, meta.start + 4, meta.end).find((box) => box.type === type)
}

exports.idatBytes = function idatBytes(data) {
  const box = exports.metaBox(data, 'idat')
  return data.subarray(box.start, box.end)
}

exports.locationOffset = function locationOffset(data, index) {
  let offset = exports.metaBox(data, 'iloc').start + 8
  for (let i = 0; i < index; i++) offset += 8 + data.readUInt16BE(offset + 6) * 8
  return offset
}

function u16(n) {
  const buffer = Buffer.alloc(2)
  buffer.writeUInt16BE(n)
  return buffer
}

function u32(n) {
  const buffer = Buffer.alloc(4)
  buffer.writeUInt32BE(n)
  return buffer
}

function box(type, ...data) {
  const body = Buffer.concat(data)
  return Buffer.concat([u32(body.length + 8), Buffer.from(type), body])
}

function full(type, version, ...data) {
  return box(type, Buffer.from([version, 0, 0, 0]), ...data)
}
