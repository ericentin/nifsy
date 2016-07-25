defmodule Nifsy.Handle do
  @moduledoc false

  defstruct [:buffer_bytes, :handle, :mode, :path]

  @opaque t :: %__MODULE__{
    buffer_bytes: pos_integer,
    handle: binary,
    mode: :read | :write,
    path: Path.t
  }

  module = __MODULE__

  defimpl Inspect do
    import Inspect.Algebra

    def inspect(nifsy, opts) do
      attrs =
        nifsy
        |> Map.from_struct()
        |> Map.delete(:handle)
        |> Keyword.new()

      concat ["#", inspect(unquote(module)), "<", to_doc(attrs, opts), ">"]
    end
  end
end
