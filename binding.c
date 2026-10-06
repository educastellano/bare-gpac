#include <assert.h>
#include <bare.h>
#include <gpac/internal/isomedia_dev.h>
#include <gpac/isomedia.h>
#include <js.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  GF_ISOFile *handle;
  GF_Blob input;
  GF_Box **boxes;
  uint32_t box_count;
} bare_gpac_file_t;

typedef struct {
  uint8_t *data;
  size_t len;
  size_t capacity;
  GF_Err error;
} bare_gpac_output_t;

static bool
bare_gpac__get_fourcc(js_env_t *env, js_value_t *value, uint32_t *type) {
  size_t len;
  if (js_get_value_string_utf8(env, value, NULL, 0, &len) < 0) {
    return false;
  }

  if (len != 4) {
    js_throw_range_error(env, NULL, "Expected a four-byte type");

    return false;
  }

  utf8_t bytes[5];
  if (js_get_value_string_utf8(env, value, bytes, sizeof(bytes), NULL) < 0) {
    return false;
  }

  *type = GF_4CC(bytes[0], bytes[1], bytes[2], bytes[3]);

  return true;
}

static void
bare_gpac__set_string(js_env_t *env, js_value_t *object, const char *key, const char *text) {
  int err;

  js_value_t *value;
  err = js_create_string_utf8(env, (const utf8_t *) (text ? text : ""), -1, &value);
  assert(err == 0);

  err = js_set_named_property(env, object, key, value);
  assert(err == 0);
}

static void
bare_gpac__set_fourcc(js_env_t *env, js_value_t *object, const char *key, uint32_t type) {
  char fourcc[5] = {type >> 24, type >> 16, type >> 8, type, 0};
  bare_gpac__set_string(env, object, key, fourcc);
}

static void
bare_gpac__set_boolean(js_env_t *env, js_value_t *object, const char *key, bool boolean) {
  int err;

  js_value_t *value;
  err = js_get_boolean(env, boolean, &value);
  assert(err == 0);

  err = js_set_named_property(env, object, key, value);
  assert(err == 0);
}

static void
bare_gpac__set_uint32(js_env_t *env, js_value_t *object, const char *key, uint32_t number) {
  int err;

  js_value_t *value;
  err = js_create_uint32(env, number, &value);
  assert(err == 0);

  err = js_set_named_property(env, object, key, value);
  assert(err == 0);
}

static js_value_t *
bare_gpac__copy_arraybuffer(js_env_t *env, const void *data, size_t len) {
  int err;

  js_value_t *result;
  void *output;
  err = js_create_arraybuffer(env, len, &output, &result);
  if (err < 0) {
    return NULL;
  }

  if (len) {
    memcpy(output, data, len);
  }

  return result;
}

static js_value_t *
bare_gpac__throw(js_env_t *env, GF_Err code) {
  int err;

  err = js_throw_error(env, "ERR_GPAC", gf_error_to_string(code));
  assert(err == 0);

  return NULL;
}

static void
bare_gpac__close(bare_gpac_file_t *self) {
  if (self->handle != NULL) {
    gf_isom_delete(self->handle);
  }

  self->handle = NULL;

  free(self->input.data);
  self->input.data = NULL;

  free(self->boxes);
  self->boxes = NULL;
  self->box_count = 0;
}

static void
bare_gpac__on_finalize(js_env_t *env, void *data, void *hint) {
  bare_gpac_file_t *self = data;

  bare_gpac__close(self);

  free(self);
}

static bare_gpac_file_t *
bare_gpac__unwrap(js_env_t *env, js_value_t *value) {
  int err;

  bare_gpac_file_t *self;
  err = js_unwrap(env, value, (void **) &self);
  assert(err == 0);

  if (self->handle == NULL) {
    js_throw_error(env, NULL, "ISO-BMFF file has been destroyed");

    return NULL;
  }

  return self;
}

static js_value_t *
bare_gpac_init(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 4;
  js_value_t *argv[4];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  assert(argc == 4);

  uint8_t *data;
  size_t buffer_cap;
  err = js_get_arraybuffer_info(env, argv[1], (void **) &data, &buffer_cap);
  assert(err == 0);

  int64_t offset;
  err = js_get_value_int64(env, argv[2], &offset);
  assert(err == 0);

  int64_t len;
  err = js_get_value_int64(env, argv[3], &len);
  assert(err == 0);

  if (offset < 0 || len < 8 || len > UINT32_MAX || (uint64_t) offset > buffer_cap || (uint64_t) len > buffer_cap - (size_t) offset) {
    return bare_gpac__throw(env, GF_BAD_PARAM);
  }

  bare_gpac_file_t *self = calloc(1, sizeof(*self));

  if (self == NULL) {
    return bare_gpac__throw(env, GF_OUT_OF_MEM);
  }

  self->input.data = malloc((size_t) len);

  if (self->input.data == NULL) {
    free(self);

    return bare_gpac__throw(env, GF_OUT_OF_MEM);
  }

  memcpy(self->input.data, data + offset, (size_t) len);
  self->input.size = (uint32_t) len;

  char url[64];
  snprintf(url, sizeof(url), "gmem://%p", (void *) &self->input);

  self->handle = gf_isom_open(url, GF_ISOM_OPEN_EDIT, NULL);

  if (self->handle == NULL) {
    GF_Err code = gf_isom_last_error(NULL);

    bare_gpac__on_finalize(env, self, NULL);

    return bare_gpac__throw(env, code);
  }

  gf_isom_keep_utc_times(self->handle, GF_TRUE);
  gf_isom_disable_inplace_rewrite(self->handle);

  err = js_wrap(env, argv[0], self, bare_gpac__on_finalize, NULL, NULL);

  if (err < 0) {
    bare_gpac__on_finalize(env, self, NULL);

    return NULL;
  }

  return argv[0];
}

static js_value_t *
bare_gpac_items(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 1;
  js_value_t *argv[1];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  assert(argc == 1);

  bare_gpac_file_t *self = bare_gpac__unwrap(env, argv[0]);

  if (self == NULL) {
    return NULL;
  }

  uint32_t count = gf_isom_get_meta_item_count(self->handle, GF_TRUE, 0);
  uint32_t primary = gf_isom_get_meta_primary_item_id(self->handle, GF_TRUE, 0);

  js_value_t *result;
  err = js_create_array_with_length(env, count, &result);
  assert(err == 0);

  for (uint32_t i = 0; i < count; i++) {
    uint32_t id;
    uint32_t type;
    uint32_t protection;
    uint32_t version;
    const char *name;
    const char *mime;
    const char *encoding;
    const char *url;
    const char *urn;

    GF_Err code = gf_isom_get_meta_item_info(self->handle, GF_TRUE, 0, i + 1, &id, &type, &protection, &version, NULL, &name, &mime, &encoding, &url, &urn);
    if (code) {
      return bare_gpac__throw(env, code);
    }

    js_value_t *item;
    err = js_create_object(env, &item);
    assert(err == 0);

    bare_gpac__set_uint32(env, item, "id", id);
    bare_gpac__set_boolean(env, item, "primary", id == primary);
    bare_gpac__set_fourcc(env, item, "type", type);
    bare_gpac__set_string(env, item, "name", name);
    bare_gpac__set_string(env, item, "contentType", mime);
    bare_gpac__set_string(env, item, "contentEncoding", encoding);

    err = js_set_element(env, result, i, item);
    assert(err == 0);
  }

  return result;
}

static js_value_t *
bare_gpac_add_item(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 9;
  js_value_t *argv[9];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  assert(argc == 9);

  bare_gpac_file_t *self = bare_gpac__unwrap(env, argv[0]);

  if (self == NULL) {
    return NULL;
  }

  uint8_t *data;
  size_t buffer_cap;
  err = js_get_arraybuffer_info(env, argv[1], (void **) &data, &buffer_cap);
  assert(err == 0);

  int64_t offset;
  err = js_get_value_int64(env, argv[2], &offset);
  assert(err == 0);

  int64_t len;
  err = js_get_value_int64(env, argv[3], &len);
  assert(err == 0);

  if (offset < 0 || len <= 0 || len > UINT32_MAX || (uint64_t) offset > buffer_cap || (uint64_t) len > buffer_cap - (size_t) offset) {
    return bare_gpac__throw(env, GF_BAD_PARAM);
  }

  uint32_t id;
  err = js_get_value_uint32(env, argv[4], &id);
  assert(err == 0);

  uint32_t type;
  if (!bare_gpac__get_fourcc(env, argv[5], &type)) {
    return NULL;
  }

  char *strings[3] = {NULL, NULL, NULL};
  GF_Err code = GF_OK;

  for (size_t i = 0; i < 3; i++) {
    size_t string_len;
    err = js_get_value_string_utf8(env, argv[6 + i], NULL, 0, &string_len);
    assert(err == 0);

    strings[i] = malloc(string_len + 1);

    if (strings[i] == NULL) {
      code = GF_OUT_OF_MEM;

      break;
    }

    err = js_get_value_string_utf8(env, argv[6 + i], (utf8_t *) strings[i], string_len + 1, NULL);
    assert(err == 0);
  }

  if (!code) {
    code = gf_isom_add_meta_item_memory(self->handle, GF_TRUE, 0, strings[0], &id, type, strings[1][0] ? strings[1] : NULL, strings[2][0] ? strings[2] : NULL, NULL, (char *) data + offset, (uint32_t) len, NULL);
  }

  for (size_t i = 0; i < 3; i++) free(strings[i]);

  if (code) {
    return bare_gpac__throw(env, code);
  }

  js_value_t *result;
  err = js_create_uint32(env, id, &result);
  assert(err == 0);

  return result;
}

static js_value_t *
bare_gpac_read_item(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 2;
  js_value_t *argv[2];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  assert(argc == 2);

  bare_gpac_file_t *self = bare_gpac__unwrap(env, argv[0]);

  if (self == NULL) {
    return NULL;
  }

  uint32_t id;
  err = js_get_value_uint32(env, argv[1], &id);
  assert(err == 0);

  uint32_t index = gf_isom_get_meta_item_by_id(self->handle, GF_TRUE, 0, id);

  if (index) {
    GF_ItemInfoEntryBox *infe = gf_list_get(self->handle->meta->item_infos->item_infos, index - 1);

    if (infe->full_path != NULL && infe->data_len) {
      return bare_gpac__copy_arraybuffer(env, infe->full_path, infe->data_len);
    }
  }

  uint8_t *data = NULL;
  uint32_t len = 0;
  uint32_t capacity = 0;

  GF_Err code = gf_isom_extract_meta_item_mem(self->handle, GF_TRUE, 0, id, &data, &len, &capacity, NULL, GF_FALSE);

  if (code == GF_OK && data == NULL) {
    code = GF_NOT_SUPPORTED;
  }

  if (code) {
    gf_free(data);

    return bare_gpac__throw(env, code);
  }

  js_value_t *result = bare_gpac__copy_arraybuffer(env, data, len);

  gf_free(data);

  return result;
}

typedef struct {
  uint64_t start;
  uint64_t len;
} bare_gpac_range_t;

static GF_Err
bare_gpac__item_range(bare_gpac_file_t *self, GF_MediaDataBox *idat, GF_ItemLocationEntry *item, GF_ItemExtentEntry *extent, bare_gpac_range_t *range) {
  if (item->data_reference_index || item->construction_method > 1 || extent->extent_index) {
    return GF_NOT_SUPPORTED;
  }

  uint64_t origin = item->construction_method == 1 ? idat->bsOffset : 0;
  uint64_t limit = item->construction_method == 1 ? idat->dataSize : self->input.size;

  if (item->base_offset > limit || extent->extent_offset > limit - item->base_offset) {
    return GF_ISOM_INVALID_FILE;
  }

  uint64_t offset = item->base_offset + extent->extent_offset;
  uint64_t len = extent->extent_length;

  if (!len) {
    if (gf_list_count(item->extent_entries) != 1) {
      return GF_NOT_SUPPORTED;
    }
    len = limit - offset;
  }

  if (len > limit - offset) {
    return GF_ISOM_INVALID_FILE;
  }

  range->start = origin + offset;
  range->len = len;

  return GF_OK;
}

static GF_Err
bare_gpac__erase_item_data(bare_gpac_file_t *self, uint32_t id) {
  GF_MetaBox *meta = self->handle->meta;

  if (meta == NULL || meta->item_infos == NULL || meta->item_locations == NULL) {
    return GF_BAD_PARAM;
  }

  if (!gf_isom_get_meta_item_by_id(self->handle, GF_TRUE, 0, id)) {
    return GF_NOT_FOUND;
  }

  GF_List *locations = meta->item_locations->location_entries;
  GF_ItemLocationEntry *target = NULL;

  for (uint32_t i = 0; i < gf_list_count(locations); i++) {
    GF_ItemLocationEntry *item = gf_list_get(locations, i);

    if (item->item_ID != id) {
      continue;
    }
    if (target != NULL) {
      return GF_ISOM_INVALID_FILE;
    }

    target = item;
  }

  if (target == NULL) {
    return GF_ISOM_INVALID_FILE;
  }

  // Only idat storage needs explicit erasure; leave other removal behavior alone.
  if (target->construction_method == 0) {
    return GF_OK;
  }

  if (target->construction_method != 1 || target->data_reference_index) {
    return GF_NOT_SUPPORTED;
  }

  GF_MediaDataBox *idat = NULL;

  for (uint32_t i = 0; i < gf_list_count(meta->child_boxes); i++) {
    GF_Box *box = gf_list_get(meta->child_boxes, i);

    if (box->type != GF_ISOM_BOX_TYPE_IDAT) continue;

    if (idat != NULL) {
      return GF_ISOM_INVALID_FILE;
    }

    idat = (GF_MediaDataBox *) box;
  }

  if (idat == NULL || idat->data == NULL || idat->bsOffset > self->input.size || idat->dataSize > self->input.size - idat->bsOffset) {
    return GF_ISOM_INVALID_FILE;
  }

  uint32_t count = gf_list_count(target->extent_entries);

  if (!count) {
    return GF_ISOM_INVALID_FILE;
  }

  if ((size_t) count > SIZE_MAX / sizeof(bare_gpac_range_t)) {
    return GF_OUT_OF_MEM;
  }

  bare_gpac_range_t *ranges = malloc((size_t) count * sizeof(*ranges));

  if (ranges == NULL) {
    return GF_OUT_OF_MEM;
  }

  GF_Err code = GF_OK;

  for (uint32_t i = 0; i < count; i++) {
    code = bare_gpac__item_range(self, idat, target, gf_list_get(target->extent_entries, i), &ranges[i]);
    if (code) {
      goto done;
    }
  }

  for (uint32_t i = 0; i < gf_list_count(locations); i++) {
    GF_ItemLocationEntry *item = gf_list_get(locations, i);

    if (item == target) continue;

    if (item->construction_method > 1 || item->data_reference_index) {
      code = GF_NOT_SUPPORTED;
      goto done;
    }

    for (uint32_t j = 0; j < gf_list_count(item->extent_entries); j++) {
      bare_gpac_range_t other;
      code = bare_gpac__item_range(self, idat, item, gf_list_get(item->extent_entries, j), &other);
      if (code) goto done;

      for (uint32_t k = 0; k < count; k++) {
        if (ranges[k].len && other.len && ranges[k].start < other.start + other.len && other.start < ranges[k].start + ranges[k].len) {
          code = GF_BAD_PARAM;
          goto done;
        }
      }
    }
  }

  // GPAC serializes its idat copy but extracts items from the owned input.
  // No bytes or item records are changed until all ranges have passed validation.
  for (uint32_t i = 0; i < count; i++) {
    memset(idat->data + (size_t) (ranges[i].start - idat->bsOffset), 0, (size_t) ranges[i].len);
    memset(self->input.data + (size_t) ranges[i].start, 0, (size_t) ranges[i].len);
  }

done:
  free(ranges);

  return code;
}

static js_value_t *
bare_gpac_erase_item_data(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 2;
  js_value_t *argv[2];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  assert(argc == 2);

  bare_gpac_file_t *self = bare_gpac__unwrap(env, argv[0]);

  if (self == NULL) {
    return NULL;
  }

  uint32_t id;
  err = js_get_value_uint32(env, argv[1], &id);
  assert(err == 0);

  if (id == gf_isom_get_meta_primary_item_id(self->handle, GF_TRUE, 0)) {
    return bare_gpac__throw(env, GF_BAD_PARAM);
  }

  GF_Err code = bare_gpac__erase_item_data(self, id);
  if (code) {
    return bare_gpac__throw(env, code);
  }

  return NULL;
}

static js_value_t *
bare_gpac_remove_item(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 3;
  js_value_t *argv[3];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  assert(argc == 3);

  bare_gpac_file_t *self = bare_gpac__unwrap(env, argv[0]);

  if (self == NULL) {
    return NULL;
  }

  uint32_t id;
  err = js_get_value_uint32(env, argv[1], &id);
  assert(err == 0);

  if (id == gf_isom_get_meta_primary_item_id(self->handle, GF_TRUE, 0)) {
    return bare_gpac__throw(env, GF_BAD_PARAM);
  }

  bool keep_refs;
  err = js_get_value_bool(env, argv[2], &keep_refs);
  assert(err == 0);

  GF_Err code = gf_isom_remove_meta_item(self->handle, GF_TRUE, 0, id, keep_refs, NULL);
  if (code) {
    return bare_gpac__throw(env, code);
  }

  return NULL;
}

static js_value_t *
bare_gpac_find_box(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 2;
  js_value_t *argv[2];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  assert(argc == 2);

  bare_gpac_file_t *self = bare_gpac__unwrap(env, argv[0]);

  if (self == NULL) {
    return NULL;
  }

  uint32_t type;
  if (!bare_gpac__get_fourcc(env, argv[1], &type)) {
    return NULL;
  }

  GF_Box *box = gf_isom_box_find_child(self->handle->TopBoxes, type);
  uint32_t id = 0;

  if (box != NULL) {
    for (uint32_t i = 0; i < self->box_count; i++) {
      if (self->boxes[i] == box) {
        id = i + 1;
        break;
      }
    }

    if (!id) {
      if (self->box_count == UINT32_MAX || (size_t) self->box_count >= SIZE_MAX / sizeof(*self->boxes)) {
        return bare_gpac__throw(env, GF_OUT_OF_MEM);
      }

      GF_Box **boxes = realloc(self->boxes, ((size_t) self->box_count + 1) * sizeof(*boxes));

      if (boxes == NULL) {
        return bare_gpac__throw(env, GF_OUT_OF_MEM);
      }

      self->boxes = boxes;
      self->boxes[self->box_count++] = box;
      id = self->box_count;
    }
  }

  js_value_t *result;

  if (box == NULL) {
    err = js_get_null(env, &result);
    assert(err == 0);

    return result;
  }

  err = js_create_object(env, &result);
  assert(err == 0);

  bare_gpac__set_uint32(env, result, "id", id);

  type = box->type == GF_ISOM_BOX_TYPE_UNKNOWN ? ((GF_UnknownBox *) box)->original_4cc : box->type;
  bare_gpac__set_fourcc(env, result, "type", type);

  return result;
}

static js_value_t *
bare_gpac_remove_box(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 2;
  js_value_t *argv[2];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  assert(argc == 2);

  bare_gpac_file_t *self = bare_gpac__unwrap(env, argv[0]);

  if (self == NULL) {
    return NULL;
  }

  uint32_t id;
  err = js_get_value_uint32(env, argv[1], &id);
  assert(err == 0);

  if (!id || id > self->box_count || self->boxes[id - 1] == NULL) {
    return bare_gpac__throw(env, GF_BAD_PARAM);
  }

  GF_Box *box = self->boxes[id - 1];

  if (box->type != GF_ISOM_BOX_TYPE_UNKNOWN) {
    return bare_gpac__throw(env, GF_NOT_SUPPORTED);
  }

  gf_isom_box_del_parent(&self->handle->TopBoxes, box);

  self->boxes[id - 1] = NULL;

  return NULL;
}

static GF_Err
bare_gpac__on_write(void *data, uint8_t *block, uint32_t len, void *sample, uint32_t magic) {
  bare_gpac_output_t *output = data;

  if (output->error) {
    return output->error;
  }

  if (len > UINT32_MAX - output->len) {
    return output->error = GF_OUT_OF_MEM;
  }

  size_t end = output->len + len;

  if (end > output->capacity) {
    size_t capacity = end > UINT32_MAX / 2 ? end : end * 2;
    uint8_t *next = realloc(output->data, capacity);

    if (next == NULL) {
      return output->error = GF_OUT_OF_MEM;
    }

    output->data = next;
    output->capacity = capacity;
  }

  if (len) {
    memcpy(output->data + output->len, block, len);
  }

  output->len = end;

  return GF_OK;
}

static js_value_t *
bare_gpac_write(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 1;
  js_value_t *argv[1];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  assert(argc == 1);

  bare_gpac_file_t *self = bare_gpac__unwrap(env, argv[0]);

  if (self == NULL) {
    return NULL;
  }

  bare_gpac_output_t output = {0};

  GF_Err code = gf_isom_set_storage_mode(self->handle, GF_ISOM_STORE_STREAMABLE);

  if (!code) {
    code = gf_isom_set_final_name(self->handle, "_gpac_isobmff_redirect");
  }

  if (!code) {
    code = gf_isom_set_write_callback(self->handle, bare_gpac__on_write, NULL, NULL, &output, 65536);
  }

  if (code) {
    return bare_gpac__throw(env, code);
  }

  code = gf_isom_close(self->handle);

  if (!code) {
    code = output.error;
  }

  self->handle = NULL;
  bare_gpac__close(self);

  if (code) {
    free(output.data);

    return bare_gpac__throw(env, code);
  }

  js_value_t *result = bare_gpac__copy_arraybuffer(env, output.data, output.len);

  free(output.data);

  return result;
}

static js_value_t *
bare_gpac_destroy(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 1;
  js_value_t *argv[1];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  assert(argc == 1);

  bare_gpac_file_t *self;
  err = js_unwrap(env, argv[0], (void **) &self);
  assert(err == 0);

  bare_gpac__close(self);

  return NULL;
}

static js_value_t *
bare_gpac_exports(js_env_t *env, js_value_t *exports) {
  int err;

#define V(name, fn) \
  { \
    js_value_t *value; \
    err = js_create_function(env, name, -1, fn, NULL, &value); \
    assert(err == 0); \
    err = js_set_named_property(env, exports, name, value); \
    assert(err == 0); \
  }

  V("init", bare_gpac_init)
  V("items", bare_gpac_items)
  V("readItem", bare_gpac_read_item)
  V("addItem", bare_gpac_add_item)
  V("removeItem", bare_gpac_remove_item)
  V("eraseItemData", bare_gpac_erase_item_data)
  V("findBox", bare_gpac_find_box)
  V("removeBox", bare_gpac_remove_box)
  V("write", bare_gpac_write)
  V("destroy", bare_gpac_destroy)
#undef V

  return exports;
}

BARE_MODULE(bare_gpac, bare_gpac_exports)
