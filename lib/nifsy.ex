defmodule Nifsy do
  @on_load {:init, 0}

  @app Mix.Project.config[:app]
  @env Mix.env

  @flags [:read, :write, :append, :create, :exclusive, :truncate, :sync, :rsync, :dsync, :lock]

  def stream!(path, read_ahead \\ 8192) do
    path = String.to_charlist(path)
    Stream.resource(
    fn ->
      {:ok, handle} = open(path, read_ahead, [:read])
      handle
    end,
    fn handle ->
      case read_line(handle) do
        :eof -> {:halt, handle}
        line -> {[line], handle}
      end
    end,
    &close/1
    )
  end

  @doc false
  def init do
    path = :filename.join(:code.priv_dir(@app), to_charlist(@env) ++ '/nifsy')
    :ok = :erlang.load_nif(path, 0)
  end

  @doc false
  def open(_path, _buffer_alloc, _options) do
    exit(:nif_library_not_loaded)
  end

  @doc false
  def read(_file_desc, _num_bytes) do
    exit(:nif_library_not_loaded)
  end

  @doc false
  def read_line(_file_desc) do
    exit(:nif_library_not_loaded)
  end

  @doc false
  def write(_file_desc, _binary) do
    exit(:nif_library_not_loaded)
  end

  @doc false
  def close(_file_desc) do
    exit(:nif_library_not_loaded)
  end
end
