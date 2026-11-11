{
  description = "shady-greeter, a greetd login screen for the Shady compositor";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  inputs.shady.url = "github:gloaming-dusk/shady";
  inputs.shady.inputs.nixpkgs.follows = "nixpkgs";
  inputs.shady-shell.url = "github:gloaming-dusk/shady-shell";
  inputs.shady-shell.inputs.nixpkgs.follows = "nixpkgs";
  inputs.shady-shell.inputs.shady.follows = "shady";

  outputs = { self, nixpkgs, shady, shady-shell, ... }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f system nixpkgs.legacyPackages.${system});
      greeterFor = system: pkgs: pkgs.callPackage ./nix/package.nix {
        shady = shady.packages.${system}.default;
        shady-shell = shady-shell.packages.${system}.default;
      };
    in {
      packages = forAllSystems (system: pkgs: rec {
        shady-greeter = greeterFor system pkgs;
        default = shady-greeter;
      });

      # `nix run github:gloaming-dusk/shady-greeter`: the login screen in a
      # window, with a fake greetd that takes any non-empty password.
      apps = forAllSystems (system: pkgs: let
        try = pkgs.writeShellScript "shady-greeter-try" ''
          export SHADY_GREETER_DEMO=1 SHADY_SESSION_LOG=stderr
          exec ${greeterFor system pkgs}/bin/shady-greeter "$@"
        '';
      in {
        default = { type = "app"; program = "${try}"; };
      });

      nixosModules = rec {
        shady-greeter = import ./nix/module.nix self;
        default = shady-greeter;
      };

      devShells = forAllSystems (system: pkgs: {
        default = pkgs.mkShell {
          inputsFrom = [ (greeterFor system pkgs) ];
          packages = [ shady.packages.${system}.default shady-shell.packages.${system}.default ];
        };
      });
    };
}
