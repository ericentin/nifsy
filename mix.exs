try do
  :erlang.system_info(:dirty_cpu_schedulers)
catch
  _, _ -> IO.warn [
    "You are using Nifsy on a BEAM without dirty schedulers. You must pass the `+sfwi Interval` ",
    "flag to set a scheduler forced wakeup interval, or very bad things could happen. See the ",
    "Nifsy README for more information."
  ], []
end

defmodule Nifsy.Mixfile do
  use Mix.Project

  @version "0.1.0"

  def project do
    [app: :nifsy,
     version: @version,
     elixir: "~> 1.3",
     build_embedded: Mix.env == :prod,
     start_permanent: Mix.env == :prod,
     compilers: [:elixir_make] ++ Mix.compilers,
     make_env: %{"MIX_ENV" => to_string(Mix.env)},
     make_clean: ["clean"],
     deps: deps(),
     description: description,
     package: package,
     source_url: "https://github.com/antipax/nifsy",
     docs: [
       main: "readme",
       extras: ["README.md"],
       source_ref: "v#{@version}"
    ]]
  end

  defp deps do
    [{:elixir_make, "~> 0.3"},
     {:ex_doc, "~> 0.13", only: :dev}]
  end

  defp description do
    "A nifty NIF for the FS, providing faster filesystem operations."
  end

  defp package do
    [
      files: ["c_src", "lib", "mix.exs", "README.md", "LICENSE", "Makefile"],
      maintainers: ["Eric Entin"],
      licenses: ["Apache 2.0"],
      links: %{
        "GitHub" => "https://github.com/antipax/nifsy"
      }
    ]
  end
end
