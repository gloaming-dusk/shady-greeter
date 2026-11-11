# NixOS module: `services.shady-greeter.enable = true;` makes the Shady
# greeter greetd's login screen.
self:
{ config, lib, pkgs, ... }:

let
  cfg = config.services.shady-greeter;
  vars = config.environment.sessionVariables;
in
{
  options.services.shady-greeter = {
    enable = lib.mkEnableOption "the Shady greeter, Tide's login screen over its night sea, as greetd's login screen";

    package = lib.mkOption {
      type = lib.types.package;
      default = self.packages.${pkgs.stdenv.hostPlatform.system}.default;
      defaultText = lib.literalExpression "shady-greeter.packages.\${system}.default";
      description = "The shady-greeter package to use.";
    };

    theme = lib.mkOption {
      type = lib.types.str;
      default = "tide";
      example = "afterglow";
      description = ''
        The login screen's look: `tide`, shipped with the greeter, or a rice
        from {option}`services.shady-greeter.themePackages` that has a
        `greeter/` directory of its own (Afterglow does).
      '';
    };

    themePackages = lib.mkOption {
      type = lib.types.listOf lib.types.package;
      default = [ ];
      example = lib.literalExpression "[ shady-rice-afterglow.packages.\${system}.default ]";
      description = ''
        Packages with rices in `share/shady/rices`, searched for
        {option}`services.shady-greeter.theme`. Their login screens run with
        access to every password typed into them.
      '';
    };

    hint = lib.mkOption {
      type = lib.types.str;
      default = "";
      example = "Live session: the nixos user has no password, press Enter.";
      description = "A line shown under the login card, e.g. how to log in to a live system.";
    };

    softwareCursor = lib.mkEnableOption "software cursors for virtual GPUs with unreliable cursor planes";
  };

  config = lib.mkIf cfg.enable {
    services.greetd = {
      enable = true;
      settings.default_session = {
        user = "greeter";
        # greetd does not pass its own environment on to the greeter.
        command = lib.concatStringsSep " " ([
          "env"
          "SHADY_SOFTWARE_CURSORS=${if cfg.softwareCursor then "1" else "0"}"
          "XCURSOR_THEME=${lib.escapeShellArg (vars.XCURSOR_THEME or "Adwaita")}"
          "XCURSOR_SIZE=${lib.escapeShellArg (toString (vars.XCURSOR_SIZE or "24"))}"
          "XCURSOR_PATH=${lib.escapeShellArg (lib.concatStringsSep ":"
            (lib.toList (vars.XCURSOR_PATH or "${pkgs.adwaita-icon-theme}/share/icons")))}"
          "WLR_RENDERER_ALLOW_SOFTWARE=${lib.escapeShellArg (vars.WLR_RENDERER_ALLOW_SOFTWARE or "0")}"
          "SHADY_GREETER_SESSIONS=${config.services.displayManager.sessionData.desktops}/share/wayland-sessions"
          "SHADY_GREETER_STATE=/var/lib/shady-greeter/last"
          "SHADY_GREETER_THEME=${lib.escapeShellArg cfg.theme}"
        ] ++ lib.optional (cfg.themePackages != [ ])
          "SHADY_RICE_PATH=${lib.makeSearchPath "share/shady/rices" cfg.themePackages}"
        ++ lib.optional (cfg.hint != "")
          "SHADY_GREETER_HINT=${lib.escapeShellArg cfg.hint}"
        ++ [
          "${cfg.package}/bin/shady-greeter"
        ]);
      };
    };
    # The last user and session, so the next login starts from them.
    systemd.tmpfiles.rules = [ "d /var/lib/shady-greeter 0700 greeter greeter -" ];
    hardware.graphics.enable = lib.mkDefault true;
    # The login card lays out text with Pango through fontconfig.
    fonts.fontconfig.enable = true;
  };
}
