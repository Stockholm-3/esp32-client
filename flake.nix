{
  description = "ESP-IDF development shell";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    esp-idf-src = {
      url = "git+https://github.com/espressif/esp-idf?ref=refs/tags/v6.0&submodules=1";
      flake = false;
    };
    nix-qemu-espressif = {
      url = "github:SFrijters/nix-qemu-espressif";
      # Don't let it pull its own nixpkgs — use ours
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = {
    self,
    nixpkgs,
    esp-idf-src,
    nix-qemu-espressif,
    ...
  }: let
    system = "x86_64-linux";
    pkgs = import nixpkgs {
      inherit system;
      overlays = [nix-qemu-espressif.overlays.default];
    };
  in {
    devShells.${system}.default = pkgs.mkShell {
      hardeningDisable = ["format"];

      buildInputs = with pkgs; [
        git
        wget
        curl
        flex
        bison
        gperf
        ccache
        dfu-util
        libffi
        ncurses
        usbutils
        cmake
        ninja
        gnumake
        pkg-config
        libusb1
        python3
        python3Packages.pip
        python3Packages.virtualenv
        libbsd
        SDL2
        qemu-esp32 # from the overlay — supports ESP32 + ESP32-S3
      ];

      shellHook = ''
        export LD_LIBRARY_PATH=${pkgs.libusb1}/lib:${pkgs.stdenv.cc.cc.lib}/lib:$LD_LIBRARY_PATH
        export IDF_PATH=${esp-idf-src}
        export IDF_TOOLS_PATH=$HOME/.espressif
        export IDF_SKIP_SYSTEM_CHECK=1
        export IDF_SKIP_CHECK_SUBMODULES=1
        export SSL_CERT_FILE=${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt

        # Tell idf.py where to find our Nix-built qemu instead of its own
        export QEMU_XTENSA_BIN=${pkgs.qemu-esp32}/bin/qemu-system-xtensa

        unset PYTHONPATH
        source $IDF_PATH/export.sh
      '';
    };
  };
}
