ccache_sources = [
  'args.c',
  'ccache.c',
  'cleanup.c',
  'compopt.c',
  'conf.c',
  'counters.c',
  'execute.c',
  'exitfn.c',
  'getopt_long.c',
  'hash.c',
  'hashtable.c',
  'hashtable_itr.c',
  'hashutil.c',
  'language.c',
  'lockfile.c',
  'main.c',
  'manifest.c',
  'mdfour.c',
  'murmurhashneutral2.c',
  'snprintf.c',
  'stats.c',
  'unify.c',
  'util.c',
]

ccache_deps = [
  m_dep,
  zlib_dep
]

ccache_lib = static_library ('ccache',
  ccache_sources,
  c_args : [config_h_flag],
  include_directories: [config_h_inc],
  install : true,
  dependencies : ccache_deps,
)

ccache_dep = declare_dependency(
  link_with : ccache_lib,
  include_directories: [config_h_inc],
  dependencies : ccache_deps
)

ccache = executable('ccache',
  ccache_sources,
  c_args : [config_h_flag],
  install : true,
  dependencies : [ccache_dep],
)
