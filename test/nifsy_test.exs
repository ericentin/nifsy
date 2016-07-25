defmodule NifsyTest do
  use ExUnit.Case
  doctest Nifsy

  setup do
    File.rm_rf!("tmp/test")
    File.mkdir_p("tmp/test")
  end

  test "read" do
    File.write!("tmp/test/test.txt", "test", [:append])
    {:ok, handle} = Nifsy.open("tmp/test/test.txt")
    assert Nifsy.read(handle, 3) == {:ok, "tes"}
    assert Nifsy.read(handle, 2) == {:ok, "t"}
  end

  test "read small buffer" do
    File.write!("tmp/test/test.txt", "test", [:append])
    {:ok, handle} = Nifsy.open("tmp/test/test.txt", :read, buffer_bytes: 1)
    assert Nifsy.read(handle, 3) == {:ok, "tes"}
    assert Nifsy.read(handle, 2) == {:ok, "t"}
  end

  test "read_line" do
    File.write!("tmp/test/test.txt", "test\ntest2", [:append])
    {:ok, handle} = Nifsy.open("tmp/test/test.txt")
    assert Nifsy.read_line(handle) == {:ok, "test"}
    assert Nifsy.read_line(handle) == {:ok, "test2"}
  end

  test "write" do
    {:ok, handle} = Nifsy.open("tmp/test/test.txt", :write, [:create])
    assert Nifsy.write(handle, "test") == :ok
    assert File.read!("tmp/test/test.txt") == ""
    assert Nifsy.flush(handle) == :ok
    assert File.read!("tmp/test/test.txt") == "test"
    assert Nifsy.write(handle, "test2") == :ok
    assert Nifsy.close(handle) == :ok
    assert File.read!("tmp/test/test.txt") == "testtest2"
  end

  test "write small buffer" do
    {:ok, handle} = Nifsy.open("tmp/test/test.txt", :write, [:create, buffer_bytes: 3])
    assert Nifsy.write(handle, "test") == :ok
    assert File.read!("tmp/test/test.txt") == "tes"
    assert Nifsy.flush(handle) == :ok
    assert File.read!("tmp/test/test.txt") == "test"
    assert Nifsy.write(handle, "test2") == :ok
    assert File.read!("tmp/test/test.txt") == "testtes"
    assert Nifsy.close(handle) == :ok
    assert File.read!("tmp/test/test.txt") == "testtest2"
  end

  test "read stream" do
    File.write!("tmp/test/test.txt", "test\ntest2", [:append])
    stream = Nifsy.stream!("tmp/test/test.txt")
    assert Enum.to_list(stream) == ["test", "test2"]
  end

  test "write stream" do
    stream = Nifsy.stream!("tmp/test/test.txt", :write, [:create])

    ["test", ["\n", "test", "2"]]
    |> Stream.into(stream)
    |> Stream.run()

    assert File.read!("tmp/test/test.txt") == "test\ntest2"
  end
end
