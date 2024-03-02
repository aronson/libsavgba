project('libsavgba', 'c',
  version: '3.3.0',
  license: 'BSL',
  meson_version: '>=0.59.0',
  default_options: ['warning_level=1', 'c_std=none'])

sources_c = [
  'src/err_def.c',
  'src/gba_flash.c',
  'src/gba_eeprom.c',
  'src/gba_sram.c',
]

includes = ['include']

dependencies = dependency('libtonc')

compile_args = []

libsavgba = static_library('savgba', sources_c,
  dependencies: dependencies,
  include_directories: includes)

libsavgba_dep = declare_dependency(
  compile_args: compile_args,
  include_directories: includes,
  link_with: libsavgba)

meson.override_dependency('libsavgba', libsavgba_dep)
