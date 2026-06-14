class HamTools < Formula
  desc "Command-line amateur-radio tools: callsign lookup, bearings, DX cluster monitors"
  homepage "https://github.com/Guru-RF/ham-tools"
  license "MIT"
  head "https://github.com/Guru-RF/ham-tools.git", branch: "master"

  # Stable release: tag one in the repo, then uncomment and fill these in.
  #   curl -sL https://github.com/Guru-RF/ham-tools/archive/refs/tags/v0.1.0.tar.gz | shasum -a 256
  # url "https://github.com/Guru-RF/ham-tools/archive/refs/tags/v0.1.0.tar.gz"
  # sha256 "REPLACE_ME"

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
  end
end
