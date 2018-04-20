project('ccache', 'c', version : '3.4.2', meson_version : '>= 0.37.1')

cc = meson.get_compiler('c')

zlib_dep = dependency ('zlib', required : false)
m_dep = cc.find_library('m', required : false)

# config.h generation
config_h_inc = include_directories('.')
config_h_flag = ['-DHAVE_CONFIG_H']
ccache_conf = configuration_data()

ccache_conf.set('CCACHE_VERSION', '"@0@"'.format (meson.project_version()))

if host_machine.system() != 'windows'
  ccache_conf.set10('_GNU_SOURCE', true)
endif

check_headers = [
  'sys/mman.h',
  'sys/wait.h',
  'pwd.h',
  'sys/time.h',
  'inttypes.h',
  'locale.h',
  'stdarg.h',
  'termios.h',
]

foreach h : check_headers
  define = 'HAVE_' + h.underscorify().to_upper()
  ccache_conf.set10(define, cc.has_header(h))
endforeach

check_functions = [
  'strtok_r',
  'vsnprintf',
  'snprintf',
  'vasprintf',
  'asprintf',
  'mkstemp',
  'gettimeofday',
  'gethostname',
  'strndup',
  'unsetenv',
  'realpath',
  'GetFinalPathNameByHandleW',
  'getpwuid',
  'utimes',
  'getopt_long',
  'localeconv',
]

foreach f : check_functions
  define = 'HAVE_' + f.underscorify().to_upper()
  ccache_conf.set10(define, cc.has_function(f))
endforeach

configure_file(
  output : 'config.h',
  input: 'config.h.meson',
  configuration : ccache_conf
)

subdir('src')
subdir('unittest')
