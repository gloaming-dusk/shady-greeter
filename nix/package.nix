{
  lib,
  stdenv,
  meson,
  ninja,
  pkg-config,
  lua5_4,
  cairo,
  shady,
  shady-shell,
}:

stdenv.mkDerivation {
  pname = "shady-greeter";
  version = "0.2.0";

  src = lib.fileset.toSource {
    root = ../.;
    fileset = lib.fileset.unions [
      ../meson.build
      ../meson_options.txt
      ../shady-greeter.in
      ../src
      ../tests
      ../themes
    ];
  };

  strictDeps = true;
  nativeBuildInputs = [ meson ninja pkg-config lua5_4 ];
  buildInputs = [ cairo shady-shell ];

  mesonBuildType = "release";
  mesonFlags = [
    (lib.mesonOption "shady" "${shady}/bin/shady")
    (lib.mesonOption "shady_shell" "${shady-shell}/bin/shady-shell")
  ];

  doCheck = true;

  meta = {
    description = "greetd login screen for the Shady compositor";
    homepage = "https://github.com/gloaming-dusk/shady-greeter";
    license = lib.licenses.asl20;
    platforms = lib.platforms.linux;
    mainProgram = "shady-greeter";
  };
}
