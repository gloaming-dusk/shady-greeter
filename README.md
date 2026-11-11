# shady-greeter

The Shady greeter is the login screen [greetd](https://sr.ht/~kennylevinsen/greetd/)
shows before anyone logs in: Tide before there are windows. The night sea and
the glass islands along the top edge are the desktop's, and the clock, avatar
and password field are where Tide's lock screen has them, so logging in only
fills in the rest.

## Try it

```sh
nix run github:gloaming-dusk/shady-greeter
```

opens the login screen in a window with a fake greetd that accepts any
non-empty password (`SHADY_GREETER_DEMO=1`).

## Using it

On NixOS, with this flake's module:

```nix
{
  inputs.shady-greeter.url = "github:gloaming-dusk/shady-greeter";
  # in your system's modules:
  imports = [ shady-greeter.nixosModules.default ];
  services.shady-greeter.enable = true;
}
```

This enables greetd with `shady-greeter` as its default session, run as the
`greeter` user, and keeps the last user and session in
`/var/lib/shady-greeter/last`. `services.shady-greeter.hint` adds a line at
the bottom of the screen, for example how to log in to a live system.
[Gloam](https://github.com/gloaming-dusk/gloam) turns all of this on for you.

The greeter offers the sessions installed on the system
(`services.displayManager.sessionPackages`); it does not install Shady's
session itself.

Elsewhere, build it with meson (it needs `shady-shell.pc` from
[shady-shell](https://github.com/gloaming-dusk/shady-shell), and runs the
`shady` and `shady-shell` it finds on `PATH`, or the ones given with
`-Dshady=` and `-Dshady_shell=`) and point greetd at the installed script:

```toml
[default_session]
command = "shady-greeter"
user = "greeter"
```

### Themes

`tide` is shipped here. A rice can bring a login screen of its own in a
`greeter/` directory next to its `config.lua`, with the same three files as
[themes/tide](themes/tide) and an optional `theme.sh` beside it for its
colours; [Afterglow](https://github.com/gloaming-dusk/shady-rice-afterglow)
does. `SHADY_GREETER_THEME=afterglow` picks it, from `$SHADY_RICE_PATH` or
`$XDG_DATA_DIRS/shady/rices`. On NixOS:

```nix
services.shady-greeter.theme = "afterglow";
services.shady-greeter.themePackages = [ shady-rice-afterglow.packages.${system}.default ];
```

A theme runs with every password typed into it, so only system directories
are searched, never a home directory.

Type the password and press Enter. Up and Down pick the user, Tab the
session, Escape clears the field. Restart and Shut down run
`systemctl reboot` / `systemctl poweroff`.

## Pieces

| | |
|---|---|
| `shady-greeter` (`shady-greeter.in`) | starts Shady with the greeter's config, `shady-shell` as its only client, and logs to the journal (`journalctl -t shady-greeter`) |
| `themes/tide/config.lua` | the compositor, with no shortcuts at all, so nothing on the login screen can start programs (even the default Escape-to-quit is unbound) |
| `themes/tide/init.lua` | empty; set so that no user's `init.lua` runs here |
| `themes/tide/shell.lua` | the login screen: a `persistent` popup with exclusive keyboard focus, drawn with Tide's `style.lua`, fonts and sea shader (copied here, so the greeter does not need Tide installed) |
| `src/greetd.c` | the `greetd` shell plugin: speaks greetd's IPC and lists accounts and sessions |

The login screen asks the plugin to authenticate; when greetd accepts the chosen
session the plugin asks the compositor to quit over its IPC socket, and
greetd starts the session once the greeter has exited.

The greeter fails closed. If the login screen cannot load, `shady-shell` exits rather
than showing the default shell and its launcher, and when `shady-shell` ends
for any reason the compositor ends with it. The `greetd` plugin, which sees
every password, is looked for in the install directory first.

## The greetd plugin

Values (read with `shell.value`):

| Value | |
|---|---|
| `greetd.users` | one `name\tdisplay name` line per regular account (UID 1000–59999 with a login shell) |
| `greetd.sessions` | one `name\tid` line per `.desktop` file in the session directories |
| `greetd.state` | `idle`, `busy`, `prompt`, `done` or `error` |
| `greetd.prompt`, `greetd.secret` | PAM's question while in `prompt`, and `"1"` when the answer should be hidden |
| `greetd.message`, `greetd.message_kind` | the last info or error text (`info` / `error`) |
| `greetd.last_user`, `greetd.last_session` | the previous login |

Actions (run with `shell.action`):

| Action | |
|---|---|
| `greetd.session(id)` | the session to start once authenticated |
| `greetd.login(user)` | start authenticating `user` |
| `greetd.respond(text)` | answer PAM; an answer given before the question arrives is kept for it, so `login` and `respond` together need a single Enter |
| `greetd.cancel()` | give up the current attempt |

Sessions come from the colon-separated directories in
`SHADY_GREETER_SESSIONS` (by default `/run/current-system/sw/share/wayland-sessions`,
`/usr/local/share/wayland-sessions` and `/usr/share/wayland-sessions`) and
are started as `sh -c "<Exec>"`. The last login is stored in
`SHADY_GREETER_STATE`, or `$XDG_STATE_HOME/shady-greeter/last`.

## Trying it

`SHADY_GREETER_DEMO=1` replaces greetd with a fake that accepts any
non-empty password, so the greeter can run nested or headless:

```sh
SHADY_GREETER_DEMO=1 WLR_BACKENDS=wayland shady-greeter
```

`SHADY_GREETER_COMPOSITOR` and `SHADY_GREETER_SHELL` run it with another
`shady` or `shady-shell` than the ones it was built with.

## Building

```sh
nix develop
meson setup build
ninja -C build
meson test -C build
```

## Licence

Apache-2.0, see [LICENSE](LICENSE).
