defmodule Nifsy do
  @on_load {:init, 0}

  @app Mix.Project.config[:app]

  def stream!(path, read_ahead \\ 4096) do
    path = String.to_charlist(path)
    Stream.resource(
    fn ->
      {:ok, handle} = open(path)
      handle
    end,
    fn handle ->
      case read_line(handle, read_ahead) do
        :eof -> {:halt, handle}
        line -> {[line], handle}
      end
    end,
    fn handle ->
      close(handle)
    end
    )
  end

  @doc false
  def init do
    path = :filename.join(:code.priv_dir(@app), 'nifsy')
    :ok = :erlang.load_nif(path, 0)
  end

  @doc false
  def open(_path) do
    exit(:nif_library_not_loaded)
  end

  @doc false
  def read(_file_desc, _num_bytes, _read_ahead) do
    exit(:nif_library_not_loaded)
  end

  @doc false
  def read_line(_file_desc, _num_bytes_per_read) do
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
