project('shady-greeter', 'c', version: '0.2.0',
  meson_version: '>=1.1',
  default_options: ['buildtype=debugoptimized', 'c_std=c11', 'warning_level=2'])

# The greeter is a shady-shell config plus the greetd shell plugin, run in
# the Shady compositor. It builds against shady-shell's plugin header and
# runs whichever shady and shady-shell it is pointed at.

shady_shell = dependency('shady-shell')
cairo = dependency('cairo')

prefix = get_option('prefix')
plugin_install_dir = join_paths(get_option('libdir'), 'shady-greeter', 'shell-plugins')
theme_install_dir = join_paths(get_option('datadir'), 'shady-greeter', 'themes')

# Logs people in through greetd for the login screen (themes/*/shell.lua).
shared_module('shady-shell-plugin-greetd',
  ['src/greetd.c', 'src/ipc/json.c'],
  include_directories: include_directories('src'),
  c_args: ['-D_POSIX_C_SOURCE=200809L'],
  dependencies: [shady_shell, cairo],
  install: true,
  install_dir: plugin_install_dir)

greetd_plugin_test = executable('greetd-plugin-test',
  ['tests/greetd-plugin-test.c', 'src/greetd.c', 'src/ipc/json.c'],
  include_directories: include_directories('src'),
  c_args: ['-D_POSIX_C_SOURCE=200809L'],
  dependencies: [shady_shell, cairo],
  install: false)
test('greetd-plugin', greetd_plugin_test)

lua = find_program('lua5.4', 'lua', required: false)
if lua.found()
  test('greeter-ui', lua,
    args: [files('tests/greeter-ui-test.lua'), meson.current_source_dir()])
endif

install_subdir('themes/tide', install_dir: theme_install_dir)

# The compositor and shell the launcher runs; by default the ones on PATH
# at build time, else whatever is on PATH when it runs.
programs = {}
foreach option, name : { 'shady': 'shady', 'shady_shell': 'shady-shell' }
  path = get_option(option)
  if path == ''
    found = find_program(name, required: false)
    path = found.found() ? found.full_path() : name
  endif
  programs += { option: path }
endforeach

conf = configuration_data()
conf.set('shady', programs['shady'])
conf.set('shady_shell', programs['shady_shell'])
conf.set('themedir', join_paths(prefix, theme_install_dir))
conf.set('shellplugindir', join_paths(prefix, plugin_install_dir))
configure_file(input: 'shady-greeter.in', output: 'shady-greeter',
  configuration: conf,
  install: true, install_dir: get_option('bindir'), install_mode: 'rwxr-xr-x')
