{
  inputs = {
    mars-std.url = "github:mars-research/mars-std";
  };

  outputs = { self, mars-std, ... }: let
    # System types to support.
    supportedSystems = [ "x86_64-linux" "aarch64-linux" ];
  in mars-std.lib.eachSystem supportedSystems (system: let
    pkgs = mars-std.legacyPackages.${system};
  in {
    devShell = pkgs.mkShell {
      nativeBuildInputs = with pkgs; [
        cmake
        gcc
        meson
        nasm
        ninja
        pkg-config
        clang-tools
        (ffmpeg_5.override { debugDeveloper = true; })
        llvmPackages_13.clang
        llvmPackages_13.llvm
        llvmPackages_13.lld
        python3
        nodePackages.bash-language-server
      ];
    };
  });
}
