{
  description = "TrenchBroom development environment";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { nixpkgs, ... }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in {
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
