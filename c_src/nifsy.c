#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#pragma clang diagnostic ignored "-Wpadded"
#include "erl_nif.h"
#pragma clang diagnostic pop

static ErlNifResourceType *NIFSY_RESOURCE;

typedef struct {
  int file_descriptor;
  int mode;
  ErlNifRWLock *rwlock;
  ErlNifBinary *buffer;
  unsigned long buffer_alloc;
  unsigned long buffer_offset;
  unsigned long buffer_size;
  bool closed;
  char padding[7];
} nifsy_handle;

#ifdef NIFSY_DEBUG
#define DEBUG_LOG(string) fprintf(stderr, "%s\n", string)
#else
#define DEBUG_LOG(string)
#endif

#define RW_UNLOCK                                                              \
  ({                                                                           \
    if (handle->rwlock != 0)                                                   \
      enif_rwlock_rwunlock(handle->rwlock);                                    \
  })

#define RW_LOCK                                                                \
  ({                                                                           \
    if (handle->rwlock != 0)                                                   \
      enif_rwlock_rwlock(handle->rwlock);                                      \
  })

#define RETURN_BADARG(code)                                                    \
  ({                                                                           \
    if (!(code)) {                                                             \
      return enif_make_badarg(env);                                            \
    }                                                                          \
  })

#define RETURN_ERROR(code, error_atom)                                         \
  ({                                                                           \
    if (!(code)) {                                                             \
      return make_error_tuple(env, (error_atom));                              \
    }                                                                          \
  })

#define HANDLE_ERROR(code, if_error, error_atom)                               \
  ({                                                                           \
    if (!(code)) {                                                             \
      (if_error);                                                              \
      return make_error_tuple(env, (error_atom));                              \
    }                                                                          \
  })

#define RETURN_ERROR_IF_NEG(code)                                              \
  ({                                                                           \
    if ((code) < 0) {                                                          \
      return make_error_tuple_errno(env);                                      \
    }                                                                          \
  })

#define HANDLE_ERROR_IF_NEG(code, if_error)                                    \
  ({                                                                           \
    if ((code) < 0) {                                                          \
      (if_error);                                                              \
      return make_error_tuple_errno(env);                                      \
    }                                                                          \
  })

#define MEMERR "enomem"

static ERL_NIF_TERM ATOM_OK;
static ERL_NIF_TERM ATOM_ERROR;
static ERL_NIF_TERM ATOM_READ;
static ERL_NIF_TERM ATOM_WRITE;
static ERL_NIF_TERM ATOM_APPEND;
static ERL_NIF_TERM ATOM_CREATE;
static ERL_NIF_TERM ATOM_EXCLUSIVE;
static ERL_NIF_TERM ATOM_TRUNCATE;
static ERL_NIF_TERM ATOM_SYNC;
static ERL_NIF_TERM ATOM_DSYNC;
static ERL_NIF_TERM ATOM_LOCK;

static mode_t NIFSY_DEFAULT_PERM = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

#ifdef ERTS_DIRTY_SCHEDULERS
#define DIRTINESS ERL_NIF_DIRTY_JOB_IO_BOUND
#else
#define DIRTINESS 0
#endif

static int nifsy_do_close(nifsy_handle *handle, bool from_dtor) {
  int result = 0;

  if (!handle->closed && handle->mode & O_WRONLY && handle->buffer_offset) {
    result = write(handle->file_descriptor, handle->buffer->data,
                   handle->buffer_offset);
  }

  if (from_dtor) {
    int result = close(handle->file_descriptor);
    if (handle->buffer) {
      enif_release_binary(handle->buffer);
    }
    return result;
  } else {
    handle->closed = true;
  }

  return result;
}

static void nifsy_dtor(ErlNifEnv *env, void *arg) {
  nifsy_handle *handle = (nifsy_handle *)arg;
  nifsy_do_close(handle, true);
  if (handle->rwlock != 0) {
    enif_rwlock_destroy(handle->rwlock);
  }
}

static int nifsy_on_load(ErlNifEnv *env, void **priv_data,
                         ERL_NIF_TERM load_info) {
  ErlNifResourceFlags flags =
      (ErlNifResourceFlags)(ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER);
  NIFSY_RESOURCE = enif_open_resource_type(env, NULL, "nifsy_resource",
                                           &nifsy_dtor, flags, NULL);

  ATOM_OK = enif_make_atom(env, "ok");
  ATOM_ERROR = enif_make_atom(env, "error");
  ATOM_READ = enif_make_atom(env, "read");
  ATOM_WRITE = enif_make_atom(env, "write");
  ATOM_APPEND = enif_make_atom(env, "append");
  ATOM_CREATE = enif_make_atom(env, "create");
  ATOM_EXCLUSIVE = enif_make_atom(env, "exclusive");
  ATOM_TRUNCATE = enif_make_atom(env, "truncate");
  ATOM_SYNC = enif_make_atom(env, "sync");
  ATOM_DSYNC = enif_make_atom(env, "dsync");
  ATOM_LOCK = enif_make_atom(env, "lock");

  return 0;
}

static ERL_NIF_TERM make_error_tuple_errno(ErlNifEnv *env) {
  return enif_make_tuple2(
      env, ATOM_ERROR,
      enif_make_tuple2(env, enif_make_int(env, errno),
                       enif_make_string(env, strerror(errno), ERL_NIF_LATIN1)));
}

static ERL_NIF_TERM make_error_tuple(ErlNifEnv *env, char *err) {
  return enif_make_tuple2(env, ATOM_ERROR, enif_make_atom(env, err));
}

static bool decode_options(ErlNifEnv *env, ERL_NIF_TERM list, int *mode,
                           bool *lock) {
  int m = 0;
  bool l = false;
  ERL_NIF_TERM head;

  while (enif_get_list_cell(env, list, &head, &list)) {
    if (enif_is_identical(head, ATOM_READ)) {
      m |= O_RDONLY;
    } else if (enif_is_identical(head, ATOM_WRITE)) {
      m |= O_WRONLY;
    } else if (enif_is_identical(head, ATOM_APPEND)) {
      m |= O_APPEND;
    } else if (enif_is_identical(head, ATOM_CREATE)) {
      m |= O_CREAT;
    } else if (enif_is_identical(head, ATOM_EXCLUSIVE)) {
      m |= O_EXCL;
    } else if (enif_is_identical(head, ATOM_TRUNCATE)) {
      m |= O_TRUNC;
    } else if (enif_is_identical(head, ATOM_SYNC)) {
      m |= O_SYNC;
    } else if (enif_is_identical(head, ATOM_DSYNC)) {
      m |= O_DSYNC;
    } else if (enif_is_identical(head, ATOM_LOCK)) {
      l = true;
    } else {
      return false;
    }
  }

  *mode = m;
  *lock = l;

  return true;
}

static ERL_NIF_TERM nifsy_open(ErlNifEnv *env, int argc,
                               const ERL_NIF_TERM argv[]) {
  char *path;
  unsigned long buffer_alloc;
  int mode;
  bool lock;

  RETURN_ERROR(path = enif_alloc(PATH_MAX), MEMERR);

  HANDLE_ERROR(enif_get_string(env, argv[0], path, PATH_MAX, ERL_NIF_LATIN1),
               { enif_free(path); }, "badarg");
  HANDLE_ERROR(enif_get_ulong(env, argv[1], &buffer_alloc),
               { enif_free(path); }, "badarg");
  HANDLE_ERROR(decode_options(env, argv[2], &mode, &lock), { enif_free(path); },
               "badarg");

  int file_descriptor = open(path, mode, NIFSY_DEFAULT_PERM);

  enif_free(path);

  RETURN_ERROR_IF_NEG(file_descriptor);

  nifsy_handle *handle =
      enif_alloc_resource(NIFSY_RESOURCE, sizeof(nifsy_handle));

  if (lock) {
    handle->rwlock = enif_rwlock_create("nifsy");
  } else {
    handle->rwlock = 0;
  }

  handle->file_descriptor = file_descriptor;
  handle->mode = mode;
  handle->closed = false;
  HANDLE_ERROR(handle->buffer = enif_alloc(sizeof(ErlNifBinary)),
               { RW_UNLOCK; }, MEMERR);
  HANDLE_ERROR(enif_alloc_binary(buffer_alloc, handle->buffer),
               {
                 enif_free(handle->buffer);
                 enif_release_resource(handle);
                 RW_UNLOCK;
               },
               MEMERR);
  handle->buffer_alloc = buffer_alloc;
  handle->buffer_offset = 0;
  handle->buffer_size = 0;

  ERL_NIF_TERM resource = enif_make_resource(env, handle);
  enif_release_resource(handle);

  return enif_make_tuple2(env, enif_make_atom(env, "ok"), resource);
}

static ERL_NIF_TERM nifsy_read(ErlNifEnv *env, int argc,
                               const ERL_NIF_TERM argv[]) {
  nifsy_handle *handle;
  unsigned long requested_bytes;

  RETURN_BADARG(
      enif_get_resource(env, argv[0], NIFSY_RESOURCE, (void **)&handle));
  RETURN_BADARG(enif_get_ulong(env, argv[1], &requested_bytes));

  RETURN_BADARG(!handle->closed);
  RETURN_BADARG(!(handle->mode & O_WRONLY));

  unsigned long buffer_alloc = handle->buffer_alloc;

  RW_LOCK;

  ErlNifBinary return_bytes;
  HANDLE_ERROR(enif_alloc_binary(requested_bytes, &return_bytes),
               { RW_UNLOCK; }, MEMERR);

  unsigned long return_bytes_offset = 0;

  if (handle->buffer && handle->buffer_offset != 0) {
    DEBUG_LOG("a buffer exists");
    unsigned char *rem_data = handle->buffer->data + handle->buffer_offset;
    unsigned long rem_data_size = handle->buffer_size - handle->buffer_offset;

    if (rem_data_size >= requested_bytes) {
      DEBUG_LOG("b enough data");
      memcpy(return_bytes.data, rem_data, requested_bytes);
      handle->buffer_offset += requested_bytes;

      RW_UNLOCK;

      return enif_make_tuple2(env, ATOM_OK,
                              enif_make_binary(env, &return_bytes));
    } else {
      DEBUG_LOG("c not enough data");
      memcpy(return_bytes.data, rem_data, rem_data_size);
      return_bytes_offset = rem_data_size;
      handle->buffer_offset = 0;
    }
  }

  while (true) {
    DEBUG_LOG("d loop start");
    unsigned long nbytes_read;
    HANDLE_ERROR_IF_NEG(
        nbytes_read = (unsigned long)read(handle->file_descriptor,
                                          handle->buffer->data, buffer_alloc),
        {
          enif_release_binary(&return_bytes);
          RW_UNLOCK;
        });

    handle->buffer_size = nbytes_read;

    if (!nbytes_read) {
      DEBUG_LOG("e no data");
      if (return_bytes_offset) {
        DEBUG_LOG("f return return_bytes");
        HANDLE_ERROR(enif_realloc_binary(&return_bytes, return_bytes_offset),
                     { RW_UNLOCK; }, MEMERR);
        RW_UNLOCK;
        return enif_make_tuple2(env, ATOM_OK,
                                enif_make_binary(env, &return_bytes));
      } else {
        DEBUG_LOG("g eof");
        enif_release_binary(&return_bytes);
        RW_UNLOCK;
        return enif_make_tuple2(env, ATOM_OK, enif_make_atom(env, "eof"));
      }
    }

    unsigned long remaining_bytes = requested_bytes - return_bytes_offset;

    if (nbytes_read >= remaining_bytes) {
      DEBUG_LOG("h enough bytes");
      memcpy(return_bytes.data + return_bytes_offset, handle->buffer->data,
             remaining_bytes);
      handle->buffer_offset += remaining_bytes;

      RW_UNLOCK;

      return enif_make_tuple2(env, ATOM_OK,
                              enif_make_binary(env, &return_bytes));
    } else {
      DEBUG_LOG("i not enough bytes");
      memcpy(return_bytes.data + return_bytes_offset, handle->buffer->data,
             nbytes_read);
      return_bytes_offset += nbytes_read;
      handle->buffer_offset = 0;
    }
  }
}

static ERL_NIF_TERM nifsy_read_line(ErlNifEnv *env, int argc,
                                    const ERL_NIF_TERM argv[]) {
  nifsy_handle *handle;

  RETURN_BADARG(
      enif_get_resource(env, argv[0], NIFSY_RESOURCE, (void **)&handle));

  RETURN_BADARG(!handle->closed);
  RETURN_BADARG(!(handle->mode & O_WRONLY));

  unsigned long buffer_alloc = handle->buffer_alloc;

  RW_LOCK;

  ErlNifBinary new_line_buffer;
  new_line_buffer.data = NULL;

  if (handle->buffer && handle->buffer_offset != 0) {
    DEBUG_LOG("a buffer exists");
    unsigned char *newline,
        *rem_data = handle->buffer->data + handle->buffer_offset;
    unsigned long rem_data_size = handle->buffer_size - handle->buffer_offset;

    if ((newline = memchr(rem_data, '\n', rem_data_size))) {
      DEBUG_LOG("b newline found");
      unsigned long line_size = (unsigned long)(newline - rem_data);

      HANDLE_ERROR(enif_alloc_binary(line_size, &new_line_buffer),
                   { RW_UNLOCK; }, MEMERR);

      memcpy(new_line_buffer.data, rem_data, line_size);
      ERL_NIF_TERM new_line_term = enif_make_binary(env, &new_line_buffer);
      handle->buffer_offset = handle->buffer_offset + line_size + 1;

      RW_UNLOCK;

      return enif_make_tuple2(env, ATOM_OK, new_line_term);
    } else {
      DEBUG_LOG("c newline not found");
      HANDLE_ERROR(enif_alloc_binary(rem_data_size, &new_line_buffer),
                   { RW_UNLOCK; }, MEMERR);

      memcpy(new_line_buffer.data, rem_data, rem_data_size);
      handle->buffer_offset = 0;
    }
  }

  while (true) {
    DEBUG_LOG("e loop start");
    unsigned long nbytes_read;
    HANDLE_ERROR_IF_NEG(
        nbytes_read = (unsigned long)read(handle->file_descriptor,
                                          handle->buffer->data, buffer_alloc),
        {
          if (new_line_buffer.data) {
            enif_release_binary(&new_line_buffer);
          }
          RW_UNLOCK;
        });

    handle->buffer_size = nbytes_read;

    if (!nbytes_read) {
      DEBUG_LOG("f no bytes read");
      if (new_line_buffer.data) {
        DEBUG_LOG("g buffer existed");
        RW_UNLOCK;
        return enif_make_tuple2(env, ATOM_OK,
                                enif_make_binary(env, &new_line_buffer));
      } else {
        DEBUG_LOG("h eof");
        RW_UNLOCK;
        return enif_make_tuple2(env, ATOM_OK, enif_make_atom(env, "eof"));
      }
    }

    unsigned char *newline;
    if ((newline = memchr(handle->buffer->data, '\n', handle->buffer_size))) {
      DEBUG_LOG("j newline found in read");
      unsigned long line_size = (unsigned long)(newline - handle->buffer->data);

      if (new_line_buffer.data) {
        DEBUG_LOG("k new line buffer existed");
        unsigned long orig_size = new_line_buffer.size;

        HANDLE_ERROR(enif_realloc_binary(&new_line_buffer,
                                         new_line_buffer.size + line_size),
                     { RW_UNLOCK; }, MEMERR);

        memcpy(new_line_buffer.data + orig_size, handle->buffer->data,
               line_size);
        ERL_NIF_TERM new_line_term = enif_make_binary(env, &new_line_buffer);
        handle->buffer_offset = handle->buffer_offset + line_size + 1;
        RW_UNLOCK;

        return enif_make_tuple2(env, ATOM_OK, new_line_term);
      } else {
        DEBUG_LOG("l new line buffer create");
        HANDLE_ERROR(enif_alloc_binary(line_size, &new_line_buffer),
                     { RW_UNLOCK; }, MEMERR);

        memcpy(new_line_buffer.data, handle->buffer->data, line_size);
        ERL_NIF_TERM new_line_term = enif_make_binary(env, &new_line_buffer);
        handle->buffer_offset = handle->buffer_offset + line_size + 1;
        RW_UNLOCK;

        return enif_make_tuple2(env, ATOM_OK, new_line_term);
      }
    } else {
      DEBUG_LOG("m newline not found");
      if (new_line_buffer.data) {
        DEBUG_LOG("n new line buffer exists");
        unsigned long orig_size = new_line_buffer.size;

        HANDLE_ERROR(
            enif_realloc_binary(&new_line_buffer,
                                new_line_buffer.size + handle->buffer_size),
            { RW_UNLOCK; }, MEMERR);

        memcpy(new_line_buffer.data + orig_size, handle->buffer->data,
               handle->buffer_size);
        handle->buffer_offset = 0;
      } else {
        DEBUG_LOG("o new line buffer not found");
        HANDLE_ERROR(enif_alloc_binary(handle->buffer_size, &new_line_buffer),
                     { RW_UNLOCK; }, MEMERR);

        memcpy(new_line_buffer.data, handle->buffer->data, handle->buffer_size);
        handle->buffer_offset = 0;
      }
    }
  }
}

static ERL_NIF_TERM nifsy_write(ErlNifEnv *env, int argc,
                                const ERL_NIF_TERM argv[]) {
  nifsy_handle *handle;
  ErlNifBinary write_binary;

  RETURN_BADARG(
      enif_get_resource(env, argv[0], NIFSY_RESOURCE, (void **)&handle));
  RETURN_BADARG(!handle->closed);
  RETURN_BADARG(handle->mode & O_WRONLY);
  RETURN_BADARG(enif_inspect_iolist_as_binary(env, argv[1], &write_binary));

  RW_LOCK;

  unsigned long remaining_buffer_bytes =
      handle->buffer->size - handle->buffer_offset;

  if (remaining_buffer_bytes > write_binary.size) {
    memcpy(handle->buffer->data + handle->buffer_offset, write_binary.data,
           write_binary.size);
    handle->buffer_offset += write_binary.size;
  } else {
    unsigned long write_binary_offset = 0,
                  write_binary_remaining = write_binary.size;

    while (write_binary_remaining) {
      memcpy(handle->buffer->data + handle->buffer_offset,
             write_binary.data + write_binary_offset, remaining_buffer_bytes);

      HANDLE_ERROR_IF_NEG(write(handle->file_descriptor, handle->buffer->data,
                                handle->buffer->size),
                          { RW_UNLOCK; });

      write_binary_remaining -= remaining_buffer_bytes;
      write_binary_offset += remaining_buffer_bytes;

      if (write_binary_remaining < handle->buffer->size) {
        memcpy(handle->buffer->data, write_binary.data + write_binary_offset,
               write_binary_remaining);
        handle->buffer_offset = write_binary_remaining;
        write_binary_remaining = 0;
      } else {
        handle->buffer_offset = 0;
        remaining_buffer_bytes = handle->buffer->size;
      }
    }
  }

  RW_UNLOCK;

  return ATOM_OK;
}

static ERL_NIF_TERM nifsy_flush(ErlNifEnv *env, int argc,
                                const ERL_NIF_TERM argv[]) {
  nifsy_handle *handle;
  RETURN_BADARG(
      enif_get_resource(env, argv[0], NIFSY_RESOURCE, (void **)&handle));
  RETURN_BADARG(!handle->closed);
  RETURN_BADARG(handle->mode & O_WRONLY);

  RW_LOCK;

  if (handle->buffer_offset) {
    HANDLE_ERROR_IF_NEG(write(handle->file_descriptor, handle->buffer->data,
                              handle->buffer_offset),
                        { RW_UNLOCK; });
    handle->buffer_offset = 0;
  }

  RW_UNLOCK;

  return ATOM_OK;
}

static ERL_NIF_TERM nifsy_close(ErlNifEnv *env, int argc,
                                const ERL_NIF_TERM argv[]) {
  nifsy_handle *handle;
  RETURN_BADARG(
      enif_get_resource(env, argv[0], NIFSY_RESOURCE, (void **)&handle));

  RW_LOCK;
  int ret = nifsy_do_close(handle, false);
  RW_UNLOCK;

  RETURN_ERROR_IF_NEG(ret);

  return ATOM_OK;
}

static ErlNifFunc nif_funcs[] = {{"open", 3, nifsy_open, DIRTINESS},
                                 {"read", 2, nifsy_read, DIRTINESS},
                                 {"read_line", 1, nifsy_read_line, DIRTINESS},
                                 {"write", 2, nifsy_write, DIRTINESS},
                                 {"flush", 1, nifsy_flush, DIRTINESS},
                                 {"close", 1, nifsy_close, DIRTINESS}};

ERL_NIF_INIT(Elixir.Nifsy.Native, nif_funcs, &nifsy_on_load, NULL, NULL, NULL)
