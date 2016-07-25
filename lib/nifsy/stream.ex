defmodule Nifsy.Stream do
  @moduledoc false

  defstruct [:options, :path]
  @opaque t :: %__MODULE__{
    options: Nifsy.options,
    path: Path.t
  }

  defimpl Collectable do
    def into(%{options: options, path: path} = stream) do
      case Nifsy.open(path, :write, options) do
        {:ok, handle} ->
          {:ok, into(handle.handle, stream)}

        {:error, reason} ->
          raise "could not stream #{path}: #{inspect(reason)}"
      end
    end

    def into(handle, stream) do
      fn
        :ok, {:cont, x} ->
          Nifsy.Native.write(handle, x)

        :ok, :done ->
          :ok = Nifsy.Native.close(handle)
          stream

        :ok, :halt ->
          :ok = Nifsy.Native.close(handle)
      end
    end
  end
end
