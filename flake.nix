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
        llvm
        llvmPackages.clang-tools
        libclang
        clang-tools
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
        export QEMU_XTENSA_BIN=${pkgs.qemu-esp32}/bin/qemu-system-xtensa
        export PATH=${pkgs.clang-tools}/bin:$PATH
        unset PYTHONPATH

        # Auto-install ESP-IDF Python env if missing (first-time or new machine)
        IDF_PYTHON_ENV="$IDF_TOOLS_PATH/python_env/idf6.0_py3.13_env"
        if [ ! -f "$IDF_PYTHON_ENV/bin/python" ]; then
          echo "ESP-IDF Python env not found — running install.py..."
          python3 $IDF_PATH/tools/idf_tools.py install --targets esp32,esp32s3
          python3 $IDF_PATH/tools/idf_tools.py install-python-env
        fi

        source $IDF_PATH/export.sh
      '';
    };
  };
}
