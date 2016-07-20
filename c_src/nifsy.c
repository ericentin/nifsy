#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "erl_nif.h"

static ErlNifResourceType* NIFSY_RESOURCE;

typedef struct {
  int file_descriptor;
  bool closed;
  ErlNifRWLock *rwlock;
  ErlNifBinary *read_buffer;
  unsigned long read_buffer_offset;
} nifsy_handle;

static int on_load(ErlNifEnv*, void**, ERL_NIF_TERM);

static int do_nifsy_close(nifsy_handle *handle, bool from_dtor)
{
  if (from_dtor) {
    int result = close(handle->file_descriptor);
    if (handle->read_buffer) {
      enif_release_binary(handle->read_buffer);
    }
    return result;
  } else {
    handle->closed = true;
  }

  return 0;
}

void nifsy_dtor(ErlNifEnv* env, void* arg)
{
  nifsy_handle *handle = (nifsy_handle*)arg;
  do_nifsy_close(handle, true);
  if (handle->rwlock != 0) {
    enif_rwlock_destroy(handle->rwlock);
  }
}

// #define DEBUG

#ifdef DEBUG
  #define DEBUG_LOG(format, string) printf(format, string)
#else
  #define DEBUG_LOG(format, string) NULL
#endif

#define RW_UNLOCK if (handle->rwlock != 0) enif_rwlock_rwunlock(handle->rwlock)
#define RW_LOCK   if (handle->rwlock != 0) enif_rwlock_rwlock(handle->rwlock)
#define R_UNLOCK if (handle->rwlock != 0) enif_rwlock_runlock(handle->rwlock)
#define R_LOCK   if (handle->rwlock != 0) enif_rwlock_rlock(handle->rwlock)

#define RETURN_BADARG(code) ({ \
  if (!(code)) { \
    return enif_make_badarg(env); \
  } \
})

#define RETURN_ERROR(code, error_atom) ({ \
  if (!(code)) { \
    return make_error_tuple(env, (error_atom)); \
  } \
})

#define HANDLE_ERROR(code, if_error, error_atom) ({ \
  if (!(code)) { \
    (if_error); \
    return make_error_tuple(env, (error_atom)); \
  } \
})

#define RETURN_ERROR_IF_NEG(code, error_atom) ({ \
  if ((code) < 0) { \
    return make_error_tuple(env, (error_atom)); \
  } \
})

#define HANDLE_ERROR_IF_NEG(code, if_error, error_atom) ({ \
  if ((code) < 0) { \
    (if_error); \
    return make_error_tuple(env, (error_atom)); \
  } \
})

#define MEMERR "memerr"

static ERL_NIF_TERM ATOM_OK;
static ERL_NIF_TERM ATOM_ERROR;

static ERL_NIF_TERM nifsy_open(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]);
static ERL_NIF_TERM nifsy_read(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]);
static ERL_NIF_TERM nifsy_read_line(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]);
static ERL_NIF_TERM nifsy_write(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]);
static ERL_NIF_TERM nifsy_close(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]);

#define DIRTINESS 0
// #define DIRTINESS ERL_NIF_DIRTY_JOB_IO_BOUND

static ErlNifFunc nif_funcs[] = {
  {"open", 1, nifsy_open, DIRTINESS},
  {"read", 3, nifsy_read, DIRTINESS},
  {"read_line", 2, nifsy_read_line, DIRTINESS},
  {"write", 2, nifsy_write, DIRTINESS},
  {"close", 1, nifsy_close, DIRTINESS}
};

ERL_NIF_INIT(Elixir.Nifsy, nif_funcs, &on_load, NULL, NULL, NULL)

static int on_load(ErlNifEnv *env, void **priv_data, ERL_NIF_TERM load_info)
{
    ErlNifResourceFlags flags = (ErlNifResourceFlags)(ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER);
    NIFSY_RESOURCE = enif_open_resource_type(env, NULL, "nifsy_resource", &nifsy_dtor, flags, NULL);

    ATOM_OK = enif_make_atom(env, "ok");
    ATOM_ERROR = enif_make_atom(env, "error");

    return 0;
}

static ERL_NIF_TERM make_error_tuple(ErlNifEnv *env, char *err) {
  return enif_make_tuple2(env, ATOM_ERROR, enif_make_atom(env, err));
}

static ERL_NIF_TERM nifsy_open(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  char *path;

  RETURN_ERROR(path = enif_alloc(PATH_MAX), MEMERR);

  RETURN_BADARG(enif_get_string(env, argv[0], path, PATH_MAX, ERL_NIF_LATIN1));

  // TODO: handle flags
  int file_descriptor = open(path, O_RDONLY);

  enif_free(path);

  RETURN_ERROR_IF_NEG(file_descriptor, "retval");

  nifsy_handle *handle = enif_alloc_resource(NIFSY_RESOURCE, sizeof(nifsy_handle));

  // TODO: create rwlock if requested via flags
  handle->rwlock = 0;
  handle->file_descriptor = file_descriptor;
  handle->closed = false;
  handle->read_buffer = NULL;
  handle->read_buffer_offset = 0;

  ERL_NIF_TERM resource = enif_make_resource(env, handle);
  enif_release_resource(handle);

  return enif_make_tuple2(env, enif_make_atom(env, "ok"), resource);
}

static unsigned long set_up_buffer(ErlNifEnv *env, nifsy_handle *handle, unsigned long add_length) {
  unsigned long offset;

  if (handle->read_buffer) {
    offset = handle->read_buffer->size;
    unsigned long new_size = handle->read_buffer->size + add_length;
    HANDLE_ERROR(enif_realloc_binary(handle->read_buffer, new_size), {RW_UNLOCK;}, MEMERR);
  } else {
    offset = 0;
    HANDLE_ERROR(handle->read_buffer = enif_alloc(sizeof(ErlNifBinary)), {RW_UNLOCK;}, MEMERR);
    HANDLE_ERROR(enif_alloc_binary(add_length, handle->read_buffer), {RW_UNLOCK;}, MEMERR);
  }

  return offset;
}

static ERL_NIF_TERM nifsy_read(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  nifsy_handle *handle;
  unsigned long nbytes, read_ahead;

  RETURN_BADARG(enif_get_resource(env, argv[0], NIFSY_RESOURCE, (void**)&handle));
  RETURN_BADARG(enif_get_ulong(env, argv[1], &nbytes));
  RETURN_BADARG(enif_get_ulong(env, argv[2], &read_ahead));

  RW_LOCK;

  ErlNifBinary binary;
  HANDLE_ERROR(enif_alloc_binary(nbytes, &binary), {RW_UNLOCK;}, MEMERR);

  int nbytes_read;
  HANDLE_ERROR_IF_NEG(nbytes_read = read(handle->file_descriptor, binary.data, nbytes), {
    RW_UNLOCK;
    enif_release_binary(&binary);
  }, "readerr");

  HANDLE_ERROR(enif_realloc_binary(&binary, nbytes_read), {
    RW_UNLOCK;
    enif_release_binary(&binary);
  }, "retval");

  RW_UNLOCK;
  return enif_make_tuple2(env, enif_make_atom(env, "ok"), enif_make_binary(env, &binary));
}

static ERL_NIF_TERM nifsy_read_line(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  nifsy_handle *handle;
  unsigned long read_ahead;

  RETURN_BADARG(enif_get_resource(env, argv[0], NIFSY_RESOURCE, (void**)&handle));
  RETURN_BADARG(enif_get_ulong(env, argv[1], &read_ahead));

  RW_LOCK;

  ErlNifBinary *new_line_buffer = NULL;

  if (handle->read_buffer && handle->read_buffer_offset != 0) {
    DEBUG_LOG("%s\n", "a buffer exists");
    unsigned char *newline, *rem_data = handle->read_buffer->data + handle->read_buffer_offset;
    unsigned long rem_data_size = handle->read_buffer->size - handle->read_buffer_offset;

    HANDLE_ERROR(new_line_buffer = enif_alloc(sizeof(ErlNifBinary)), {RW_UNLOCK;}, MEMERR);

    if ((newline = memchr(rem_data, '\n', rem_data_size))) {
      DEBUG_LOG("%s\n", "b newline found");
      unsigned long line_size = newline - rem_data;

      HANDLE_ERROR(enif_alloc_binary(line_size, new_line_buffer), {
        enif_free(new_line_buffer);
        RW_UNLOCK;
      }, MEMERR);

      memcpy(new_line_buffer->data, rem_data, line_size);
      ERL_NIF_TERM new_line_term = enif_make_binary(env, new_line_buffer);
      handle->read_buffer_offset = handle->read_buffer_offset + line_size + 1;
      RW_UNLOCK;

      return new_line_term;
    } else {
      DEBUG_LOG("%s\n", "c newline not found");
      HANDLE_ERROR(enif_alloc_binary(rem_data_size, new_line_buffer), {
        enif_free(new_line_buffer);
        RW_UNLOCK;
      }, MEMERR);

      memcpy(new_line_buffer->data, rem_data, rem_data_size);
      enif_release_binary(handle->read_buffer);
      enif_free(handle->read_buffer);
      handle->read_buffer = NULL;
      handle->read_buffer_offset = 0;
    }
  }

  if (!handle->read_buffer) {
    DEBUG_LOG("%s\n", "d buffer create");
    HANDLE_ERROR(handle->read_buffer = enif_alloc(sizeof(ErlNifBinary)), {RW_UNLOCK;}, MEMERR);
    HANDLE_ERROR(enif_alloc_binary(read_ahead, handle->read_buffer), {
      if (new_line_buffer) {
        enif_release_binary(new_line_buffer);
        enif_free(new_line_buffer);
      }
      enif_free(handle->read_buffer);
      RW_UNLOCK;
    }, MEMERR);
    handle->read_buffer_offset = 0;
  }

  while (true) {
    DEBUG_LOG("%s\n", "e loop start");
    unsigned long nbytes_read;
    HANDLE_ERROR_IF_NEG(nbytes_read = read(handle->file_descriptor, handle->read_buffer->data, read_ahead), {
      if (new_line_buffer) {
        enif_release_binary(new_line_buffer);
        enif_free(new_line_buffer);
      }
      RW_UNLOCK;
    }, MEMERR);

    if (!nbytes_read) {
      DEBUG_LOG("%s\n", "f no bytes read");
      if (new_line_buffer) {
        DEBUG_LOG("%s\n", "g buffer existed");
        RW_UNLOCK;
        return enif_make_binary(env, new_line_buffer);
      } else {
        DEBUG_LOG("%s\n", "h eof");
        RW_UNLOCK;
        return enif_make_atom(env, "eof");
      }
    }

    if (nbytes_read < read_ahead) {
      DEBUG_LOG("%s\n", "i less bytes read");
      HANDLE_ERROR(enif_realloc_binary(handle->read_buffer, nbytes_read), {
        if (new_line_buffer) {
          enif_release_binary(new_line_buffer);
          enif_free(new_line_buffer);
        }
        RW_UNLOCK;
      }, MEMERR);
    }

    unsigned char *newline;
    if ((newline = memchr(handle->read_buffer->data, '\n', handle->read_buffer->size))) {
      DEBUG_LOG("%s\n", "j newline found in read");
      unsigned long line_size = newline - handle->read_buffer->data;

      if (new_line_buffer) {
        DEBUG_LOG("%s\n", "k new line buffer existed");
        unsigned long orig_size = new_line_buffer->size;

        HANDLE_ERROR(enif_realloc_binary(new_line_buffer, new_line_buffer->size + line_size), {
          enif_free(new_line_buffer);
          RW_UNLOCK;
        }, MEMERR);

        memcpy(new_line_buffer->data + orig_size, handle->read_buffer->data, line_size);
        ERL_NIF_TERM new_line_term = enif_make_binary(env, new_line_buffer);
        handle->read_buffer_offset = handle->read_buffer_offset + line_size + 1;
        RW_UNLOCK;

        return new_line_term;
      } else {
        DEBUG_LOG("%s\n", "l new line buffer create");
        HANDLE_ERROR(new_line_buffer = enif_alloc(sizeof(ErlNifBinary)), {RW_UNLOCK;}, MEMERR);
        HANDLE_ERROR(enif_alloc_binary(line_size, new_line_buffer), {
          enif_free(new_line_buffer);
          RW_UNLOCK;
        }, MEMERR);

        memcpy(new_line_buffer->data, handle->read_buffer->data, line_size);
        ERL_NIF_TERM new_line_term = enif_make_binary(env, new_line_buffer);
        handle->read_buffer_offset = handle->read_buffer_offset + line_size + 1;
        RW_UNLOCK;

        return new_line_term;
      }
    } else {
      DEBUG_LOG("%s\n", "m newline not found");
      if (new_line_buffer) {
        DEBUG_LOG("%s\n", "n new line buffer exists");
        unsigned long orig_size = new_line_buffer->size;

        HANDLE_ERROR(enif_realloc_binary(new_line_buffer, new_line_buffer->size + handle->read_buffer->size), {
          enif_free(new_line_buffer);
          RW_UNLOCK;
        }, MEMERR);

        memcpy(new_line_buffer->data + orig_size, handle->read_buffer->data, handle->read_buffer->size);
        handle->read_buffer_offset = 0;
      } else {
        DEBUG_LOG("%s\n", "o new line buffer not found");
        HANDLE_ERROR(new_line_buffer = enif_alloc(sizeof(ErlNifBinary)), {RW_UNLOCK;}, MEMERR);
        HANDLE_ERROR(enif_alloc_binary(handle->read_buffer->size, new_line_buffer), {
          enif_free(new_line_buffer);
          RW_UNLOCK;
        }, MEMERR);

        memcpy(new_line_buffer->data, handle->read_buffer->data, handle->read_buffer->size);
        handle->read_buffer_offset = 0;
      }
    }
  }
}

static ERL_NIF_TERM nifsy_write(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  return enif_make_atom(env, "undef");
}

static ERL_NIF_TERM nifsy_close(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  return enif_make_atom(env, "undef");
}
