defmodule Nifsy do
  alias Nifsy.{Handle, Native}

  @default_buffer_bytes 64 * 1024

  @options [:append, :create, :dsync, :exclusive, :lock, :sync, :truncate]

  @type option ::
    :append |
    {:buffer_bytes, pos_integer} |
    :create |
    :dsync |
    :exclusive |
    :lock |
    :sync |
    :truncate

  @type mode :: :read | :write

  @type options :: [option]

  @doc """
  Open the file at `path` for `mode` operations with `options`.

  A handle is returned, which you should treat as an opaque term and not attempt to modify in any
  way.

  A handle can be opened in either `:read` or `:write` mode. This mode cannot be changed after
  open, and a handle can only be used for the operations associated with its mode.

  The option `{:buffer_bytes, pos_integer}` may be provided to alter the buffer size (which is used
  for all operations) from the default, which is 64KiB.

  The option `:lock` may be provided, which will cause all operations on the given file descriptor
  to be mutually exclusive. If you plan to use a single handle from multiple OTP processes, you
  should use this option. However, performance may be better if you can perform all operations on a
  handle from a single process and then distribute the data to multiple processes.

  The rest of the options: `:append`, `:create`, `:dsync`, `:exclusive`, `:sync`, and `:truncate`,
  behave according to their corresponding options in the
  [POSIX specification](http://pubs.opengroup.org/onlinepubs/9699919799/).

  Given the low level nature of this API, behaviour, error codes, and error messages may not be
  perfectly identical across operating systems.
  """
  @spec open(Path.t, mode, options) :: {:ok, Handle.t} | {:error, term}
  def open(path, mode \\ :read, options \\ [])
  when is_binary(path) and mode in [:read, :write] and is_list(options) do
    path_charlist = to_charlist(path)
    case init_options(options) do
      {:ok, {options, buffer_bytes}} ->
        buffer_bytes = if buffer_bytes, do: buffer_bytes, else: @default_buffer_bytes
        case Native.open(path_charlist, buffer_bytes, [mode | options]) do
          {:ok, handle} ->
            {:ok, %Handle{buffer_bytes: buffer_bytes, handle: handle, mode: mode, path: path}}
          {:error, reason} ->
            {:error, reason}
        end

      {:error, error} ->
        {:error, error}
    end
  end

  @doc """
  Read (at most) `bytes` bytes from `handle`.

  The handle must be in `:read` mode.

  A call will return `bytes` bytes, or fewer, if, for instance, there are less bytes than
  requested left in the file.

  After the last byte has been returned, the function will return `{:ok, :eof}`.
  """
  @spec read(Handle.t, pos_integer) :: {:ok, binary | :eof} | {:error, term}
  def read(%Handle{mode: :read} = handle, bytes)
  when is_integer(bytes) and bytes > 0 do
    Native.read(handle.handle, bytes)
  end

  @doc """
  Read a line from `handle`.

  The handle must be in `:read` mode.

  Lines are considered delimited by `"\\\\n"`, the delimiter is not returned, and using this
  function with files with CRLF line endings will return the CR at the end of the line.

  After the final line has been returned, the function will return `{:ok, :eof}`.
  """
  @spec read_line(Handle.t) :: {:ok, binary | :eof} | {:error, term}
  def read_line(%Handle{mode: :read} = handle) do
    Native.read_line(handle.handle)
  end

  @doc """
  Write `data` to `handle`.

  The handle must be in `:write` mode.

  Since Nifsy uses buffered IO, the data may not actually be written to the file immediately. If
  you wish to force the data to be written, you can use `flush/1` or `close/1`.
  """
  @spec write(Handle.t, iodata) :: :ok | {:error, term}
  def write(%Handle{mode: :write} = handle, data)
  when is_binary(data) do
    Native.write(handle.handle, data)
  end

  @doc """
  Flush `handle`.

  The handle must be in `:write` mode.

  It is not required to explicitly flush the handle, as it will be automatically flushed when it is
  closed/garbage collected.
  """
  @spec flush(Handle.t) :: :ok | {:error, term}
  def flush(%Handle{mode: :write} = handle) do
    Native.flush(handle.handle)
  end

  @doc """
  Close `handle`.

  It is not required to explicitly close the handle, as it will be automatically closed when it is
  garbage collected.

  If the handle is in `:write` mode, the function will write any buffered data before closing the
  handle.
  """
  @spec close(Handle.t) :: :ok | {:error, term}
  def close(%Handle{} = handle) do
    Native.close(handle.handle)
  end

  def stream!(path, mode \\ :read, options \\ [])

  def stream!(path, :read, options) do
    Stream.resource fn ->
      {:ok, handle} = open(path, :read, options)
      handle.handle
    end,
    fn handle ->
      case Native.read_line(handle) do
        {:ok, :eof} -> {:halt, handle}
        {:ok, line} -> {[line], handle}
      end
    end,
    &Native.close/1
  end

  defp init_options(options) do
    Enum.reduce_while options, {:ok, {[], nil}}, fn
      {:buffer_bytes, buffer_bytes}, {:ok, {options, curr_buffer_bytes}}
      when is_integer(buffer_bytes) and buffer_bytes > 0 ->
        {:cont, {:ok, {options, if(curr_buffer_bytes, do: curr_buffer_bytes, else: buffer_bytes)}}}
      option, {:ok, {options, buffer_bytes}} when option in @options ->
        {:cont, {:ok, {[option | options], buffer_bytes}}}
      option, _acc ->
        {:halt, {:error, "invalid option #{option}"}}
    end
  end
end
