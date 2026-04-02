{
  description = "ESP-IDF development shell";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    esp-idf-src = {
      url = "git+https://github.com/espressif/esp-idf?ref=refs/tags/v6.0&submodules=1";
      flake = false;
    };
  };

  outputs = {
    self,
    nixpkgs,
    esp-idf-src,
    ...
  }: let
    system = "x86_64-linux";
    pkgs = import nixpkgs {inherit system;};
  in {
    devShells.${system}.default = pkgs.mkShell {
      buildInputs = [
        pkgs.git
        pkgs.wget
        pkgs.curl
        pkgs.flex
        pkgs.bison
        pkgs.gperf
        pkgs.ccache
        pkgs.dfu-util
        pkgs.libffi
        pkgs.ncurses
        pkgs.usbutils
        # removed pkgs.esptool - ESP-IDF manages its own
        pkgs.cmake
        pkgs.ninja
        pkgs.gnumake
        pkgs.pkg-config
        pkgs.libusb1
        pkgs.python3
        pkgs.python3Packages.pip
        pkgs.python3Packages.virtualenv
      ];
      shellHook = ''
        export LD_LIBRARY_PATH=${pkgs.libusb1}/lib:${pkgs.stdenv.cc.cc.lib}/lib:$LD_LIBRARY_PATH
        export IDF_PATH=${esp-idf-src}
        export IDF_TOOLS_PATH=$HOME/.espressif
        export IDF_SKIP_SYSTEM_CHECK=1
        export SSL_CERT_FILE=${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt

        if [ ! -f "$IDF_TOOLS_PATH/.installed-v6.0" ]; then
          echo "Installing ESP-IDF tools for v6.0 (one-time)..."
          if $IDF_PATH/install.sh all; then
            touch "$IDF_TOOLS_PATH/.installed-v6.0"
          else
            echo "ERROR: ESP-IDF install failed. Fix the error above and re-enter the shell."
            return 1
          fi
        fi

        unset PYTHONPATH
        source $IDF_PATH/export.sh
      '';
    };
  };
}
