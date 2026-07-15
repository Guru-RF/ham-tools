class HamTools < Formula
  desc "Command-line amateur-radio tools: callsign lookup, bearings, DX cluster monitors"
  homepage "https://github.com/Guru-RF/ham-tools"
  url "https://github.com/Guru-RF/ham-tools/archive/refs/tags/v0.2.2.tar.gz"
  sha256 "2ad6c1b9ab07ce9dfc7050e858971f94afb270aec4ba190e2733096eecf0aeca"
  license "MIT"
  head "https://github.com/Guru-RF/ham-tools.git", branch: "master"

  depends_on "pkg-config" => :build
  depends_on "jansson"
  depends_on "libwebsockets"
  depends_on "libyaml"
  depends_on "readline"
  depends_on "sqlite"

  uses_from_macos "curl"
  uses_from_macos "libxml2"
  uses_from_macos "ncurses"

  def install
    # The Makefile installs every binary into $PREFIX/bin; point it at the
    # keg. READLINE_PREFIX is passed explicitly so the build does not have to
    # shell out to `brew` inside the sandbox.
    system "make", "-C", "c", "install",
           "PREFIX=#{prefix}",
           "READLINE_PREFIX=#{Formula["readline"].opt_prefix}"
  end

  test do
    # qte prints its usage to stderr and exits 1 when given no address.
    assert_match "qte <address>", shell_output("#{bin}/qte 2>&1", 1)

    # The other four tools need network/config to do anything useful, so just
    # confirm each binary was installed (catches link/install regressions).
    %w[qrz dxsummit dxheat holycluster].each { |t| assert_path_exists bin/t }
  end
end
