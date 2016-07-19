defmodule ReadNif do
  @on_load {:init, 0}

  @app Mix.Project.config[:app]

  def stream!(path, per_read \\ 4096) do
    path = String.to_charlist(path)

    Stream.resource(
      fn ->
        pattern = :binary.compile_pattern("\n")
        {:ok, file_desc} = open(path)
        {file_desc, [], pattern}
      end,
      fn {file_desc, buf, pattern} ->
        case read(file_desc, per_read) do
          {:ok, ""} when buf == [] ->
            {:halt, {file_desc, buf, pattern}}
          {:ok, ""} ->
            {[IO.iodata_to_binary(buf)], {file_desc, [], pattern}}
          {:ok, data} ->
            case String.split(data, pattern) do
              [no_line] -> {[], {file_desc, [buf | no_line], pattern}}
              other ->
                {[first_line | lines], no_line} = Enum.split(other, -1)
                first_line = IO.iodata_to_binary([buf | first_line])
                {[first_line | lines], {file_desc, no_line, pattern}}
            end
        end
      end,
      fn _ ->
        :ok
      end
    )
  end

  @doc false
  def init do
    path = :filename.join(:code.priv_dir(@app), 'read_nif')
    :ok = :erlang.load_nif(path, 0)
  end

  @doc false
  def open(_path) do
    exit(:nif_library_not_loaded)
  end

  @doc false
  def read(_file_desc, _num_bytes) do
    exit(:nif_library_not_loaded)
  end
end
