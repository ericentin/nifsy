#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>

#include "erl_nif.h"

static ERL_NIF_TERM open_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  char *path;

  if (!(path = malloc(PATH_MAX))) {
    return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "memerr"));
  }

  if (!enif_get_string(env, argv[0], path, PATH_MAX, ERL_NIF_LATIN1)) {
    return enif_make_badarg(env);
  }

  int filedesc = open(path, O_RDONLY);

  free(path);

  if (filedesc < 0) {
    return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "retval"));
  }

  return enif_make_tuple2(env, enif_make_atom(env, "ok"), enif_make_int(env, filedesc));
}

static ERL_NIF_TERM read_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  int filedesc, nbytes;

  if (!enif_get_int(env, argv[0], &filedesc)) {
    return enif_make_badarg(env);
  }

  if (!enif_get_int(env, argv[1], &nbytes)) {
    return enif_make_badarg(env);
  }

  ErlNifBinary binary;

  if (!(enif_alloc_binary(nbytes, &binary))) {
    return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "memerr"));
  }

  int nbytes_read = read(filedesc, binary.data, nbytes);

  if (nbytes_read < 0) {
    enif_release_binary(&binary);
    return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "retval"));
  }

  if (!enif_realloc_binary(&binary, nbytes_read)){
    enif_release_binary(&binary);
    return enif_make_tuple2(env, enif_make_atom(env, "error"), enif_make_atom(env, "retval"));
  }

  return enif_make_tuple2(env, enif_make_atom(env, "ok"), enif_make_binary(env, &binary));
}

static ErlNifFunc nif_funcs[] = {
  {"open", 1, open_nif, 0},
  {"read", 2, read_nif, 0}
};

ERL_NIF_INIT(Elixir.ReadNif, nif_funcs, NULL, NULL, NULL, NULL)
