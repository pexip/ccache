project('msvc-test', 'c', version : '0.1', meson_version : '>= 0.37.1')

flags = []

msvctest = executable('msvc-test',
  'main.c',
  c_args : flags,
  install : false,
  #dependencies : [ccache_dep],
)
