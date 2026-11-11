# Changelog

## Unreleased

- Cancel authentication safely while greetd is replying, keeping replacement
  users' passwords out of abandoned conversations. User and session selection
  wait during authentication and session handoff.
- Clear prompts and pending passwords on connection errors. Public PAM prompts
  require fresh input instead of consuming a pre-entered password, and input
  is hidden outside an active public prompt.

- The greeter has its own repository, moved out of
  [Shady](https://github.com/gloaming-dusk/shady) with its history. It builds
  against [shady-shell](https://github.com/gloaming-dusk/shady-shell)'s
  plugin header and runs the `shady` and `shady-shell` it was built with
  (`-Dshady=`, `-Dshady_shell=`, or `SHADY_GREETER_COMPOSITOR` /
  `SHADY_GREETER_SHELL` when it runs).
- Tide's look (`style.lua`, fonts, the sea shader) is kept in
  `themes/tide`, so the login screen does not need Tide installed.
- Other themes come from rices: `<rice>/greeter/` in `$SHADY_RICE_PATH` or
  `$XDG_DATA_DIRS/shady/rices`. The Afterglow login screen moved to
  [shady-rice-afterglow](https://github.com/gloaming-dusk/shady-rice-afterglow).
- NixOS: `services.shady-greeter` (`enable`, `theme`, `themePackages`,
  `hint`, `softwareCursor`) replaces `programs.shady.greeter`.
- `nix run` opens the login screen with a fake greetd.

Earlier changes are in Shady's changelog, up to its 0.2.0.
