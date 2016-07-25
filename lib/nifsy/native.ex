defmodule Nifsy.Native do
  @moduledoc false

  @on_load {:init, 0}
  @app Mix.Project.config[:app]
  @env Mix.env

  def init do
    path = :filename.join(:code.priv_dir(@app), to_charlist(@env) ++ '/nifsy')
    :ok = :erlang.load_nif(path, 0)
  end

  def open(_path, _buffer_bytes, _options) do
    exit(:nif_library_not_loaded)
  end

  def read(_handle, _bytes) do
    exit(:nif_library_not_loaded)
  end

  def read_line(_handle) do
    exit(:nif_library_not_loaded)
  end

  def write(_handle, _data) do
    exit(:nif_library_not_loaded)
  end

  def flush(_handle) do
    exit(:nif_library_not_loaded)
  end

  def close(_handle) do
    exit(:nif_library_not_loaded)
  end
end
