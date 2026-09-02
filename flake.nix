{
  description = "TrenchBroom development environment";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs, ... }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in {
      packages = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          fetchDependency = repo: rev: hash: pkgs.fetchFromGitHub {
            owner = builtins.head (nixpkgs.lib.splitString "/" repo);
            repo = builtins.elemAt (nixpkgs.lib.splitString "/" repo) 1;
            inherit rev;
            sha256 = hash;
          };
          trenchbroom = pkgs.stdenv.mkDerivation {
            pname = "trenchbroom-chaps";
            version = "0-unstable";
            src = pkgs.lib.cleanSourceWith {
              src = self;
              filter = path: type:
                let relative = pkgs.lib.removePrefix "${toString self}/" (toString path);
                in !(relative == "build" || pkgs.lib.hasPrefix "build/" relative
                  || relative == "result");
            };

            nativeBuildInputs = with pkgs; [
              cmake
              git
              ninja
              pandoc
              pkg-config
              qt6.wrapQtAppsHook
            ];

            buildInputs = with pkgs; [
              freeglut
              glew
              libGL
              libGLU
              libdwarf
              libxkbcommon
              qt6.qtbase
              qt6.qttools
              zlib
              zstd
            ];

            cmakeFlags = [
              "-GNinja"
              "-DBUILD_TESTING=OFF"
              "-DCMAKE_BUILD_TYPE=Release"
              "-DCPPTRACE_USE_EXTERNAL_LIBDWARF=ON"
              "-DCPPTRACE_USE_EXTERNAL_ZSTD=ON"
              "-DCPPTRACE_FIND_LIBDWARF_WITH_PKGCONFIG=ON"
            ];

            buildPhase = ''
              runHook preBuild
              cmake --build . --target TrenchBroom --parallel "$NIX_BUILD_CORES"
              runHook postBuild
            '';

            installPhase = ''
              runHook preInstall
              cmake --install .
              runHook postInstall
            '';

            preConfigure = ''
              for dependency in assimp cpptrace miniz; do
                sourceVariable="CPM_''${dependency}_SOURCE"
                sourcePath="''${!sourceVariable}"
                writablePath="$TMPDIR/''${dependency}-source"
                cp -r "$sourcePath" "$writablePath"
                chmod -R u+w "$writablePath"
                export "$sourceVariable=$writablePath"
              done
            '';

            "CPM_assimp_SOURCE" = fetchDependency "assimp/assimp" "v6.0.2"
              "1rhh0s9k1l97p7avcgyh60ja1bsbz5y59i2631xvv272xlmnl6wb";
            "CPM_Catch2_SOURCE" = fetchDependency "catchorg/Catch2" "v3.10.0"
              "0wb4dq6mr7nyz7x7g3s3xh7di7w0pjngkn1nbxib8p0yfg6amskr";
            "CPM_cpptrace_SOURCE" = fetchDependency "jeremy-rifkin/cpptrace" "v1.0.4"
              "11lnfjgch6849p9ajk2j6pi0xfwnn5a9d0g1jnq34s9m8l80jq1a";
            "CPM_compile-time-regular-expressions_SOURCE" = fetchDependency
              "hanickadot/compile-time-regular-expressions" "v3.10.0"
              "1yzdqzv4hxxp9gpfpk3azf68fqgk6ad6cim61ysydwx35rk2i3pz";
            "CPM_fmt_SOURCE" = fetchDependency "fmtlib/fmt" "11.2.0"
              "0x8j1k1cnmvv5hbhhyfm7bqw2d2rb3jpmz6bc4a195z8pzj582dh";
            "CPM_FreeImage_SOURCE" = fetchDependency "danoli3/FreeImage" "3.19.11"
              "00i91drgc15daynhschgp0w1pcb0hyyxfmqylf1vwsppn2ayjjjy";
            "CPM_freetype_SOURCE" = fetchDependency "freetype/freetype" "VER-2-13-3"
              "0xzprk58jcs08q5kaifkf3pvgp58xckyx9hxjrqnx0k97fa78pz2";
            "CPM_miniz_SOURCE" = fetchDependency "richgel999/miniz" "3.1.0"
              "18nijyd6x0r23x60mpdvyyjf7nfkz7ff95p6hs2x2151az7xf1hd";
            "CPM_stduuid_SOURCE" = fetchDependency "mariusbancila/stduuid" "v1.2.3"
              "1y7jgf45dydq0jlac5clnanwcc22la4y8c83d5i0rp87x2zll6ij";
            "CPM_tinyxml2_SOURCE" = fetchDependency "leethomason/tinyxml2" "9.0.0"
              "0pikrgwa15cz78a48s3bdnba2zlm08jpjkpm3y4cbvp2smr0w101";

            meta = {
              description = "Level editor for Quake-engine games with BMAT support";
              homepage = "https://github.com/Aidan-Chelig/TrenchBroom-Chaps";
              license = pkgs.lib.licenses.gpl3Plus;
              mainProgram = "trenchbroom";
              platforms = pkgs.lib.platforms.linux;
            };
          };
        in {
          inherit trenchbroom;
          default = trenchbroom;
        });

      devShells = forAllSystems (system:
        let pkgs = nixpkgs.legacyPackages.${system};
        in {
          default = pkgs.mkShell {
            nativeBuildInputs = with pkgs; [
              ccache
              clang-tools
              cmake
              git
              ninja
              pandoc
              pkg-config
            ];

            buildInputs = with pkgs; [
              freeglut
              glew
              libGL
              libGLU
              libxkbcommon
              qt6.qtbase
              qt6.qttools
              libxi
              libxrandr
              libxxf86vm
              zlib
            ];
          };
        });
    };
}
