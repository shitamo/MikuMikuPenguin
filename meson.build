project('mikumikupenguin', 'cpp',
  version : '0.2.1',
  license : 'GPL-3.0-only',
  meson_version : '>= 0.60.0',
  default_options : [
    'cpp_std=gnu++17',   # u8"" literals are compared with std::string (char8_t in C++20)
    'warning_level=1',
    'buildtype=debugoptimized',
    'default_library=static',
  ])

cxx = meson.get_compiler('cpp')
pkgdatadir = get_option('prefix') / get_option('datadir') / 'mikumikupenguin'

# ---------------------------------------------------------------- dependencies
glfw_dep   = dependency('glfw3', version : '>= 3.0')
epoxy_dep  = dependency('epoxy')
bullet_dep = dependency('bullet')
iconv_dep  = dependency('iconv')

glm_dep = dependency('glm', required : false)
if not glm_dep.found()
  if not cxx.has_header('glm/glm.hpp')
    error('GLM (OpenGL Mathematics) headers not found')
  endif
  glm_dep = declare_dependency()
endif

stb_dep = dependency('stb', required : false)
if not stb_dep.found()
  if not cxx.has_header('stb/stb_image.h')
    error('stb_image (libstb-dev) not found')
  endif
  stb_dep = cxx.find_library('stb')
endif

conf = configuration_data()
sound_opt = get_option('sound')
sound_deps = []
sound_backend = 'none'
if not sound_opt.disabled()
  sdl3_mixer = dependency('sdl3-mixer', required : false)
  sdl3 = dependency('sdl3', required : false)
  if sdl3_mixer.found() and sdl3.found()
    sound_deps = [sdl3, sdl3_mixer]
    sound_backend = 'SDL3_mixer'
    conf.set('MMP_SDL3_MIXER', 1)
  else
    sdl2_mixer = dependency('SDL2_mixer', required : false)
    sdl2 = dependency('sdl2', required : false)
    if sdl2_mixer.found() and sdl2.found()
      sound_deps = [sdl2, sdl2_mixer]
      sound_backend = 'SDL2_mixer'
    elif sound_opt.enabled()
      error('sound enabled, but neither SDL3_mixer nor SDL2_mixer was found')
    endif
  endif
endif
configure_file(output : 'config.h', configuration : conf)

# ---------------------------------------------------------------- libmmp
mmp_inc = include_directories('include', 'libmmp')
mmp_deps = [glfw_dep, epoxy_dep, bullet_dep, glm_dep, iconv_dep]

libmmp = library('mmp',
  'libmmp/pmx.cpp',
  'libmmp/vmd.cpp',
  'libmmp/motioncontroller.cpp',
  'libmmp/interpolation.cpp',
  'libmmp/bulletphysics.cpp',
  'libmmp/bulletdebugdrawer.cpp',
  'libmmp/mmdphysics.cpp',
  'libmmp/texthandle.cpp',
  'libmmp/shader.cpp',
  'libmmp/glm_helper.cpp',
  include_directories : mmp_inc,
  dependencies : mmp_deps,
  install : true)

install_headers(
  'include/libmmp.h',
  'include/mmp_gl.h',
  'include/mmp_glm.h',
  'include/bulletdebugdrawer.h',
  'include/bulletphysics.h',
  'include/mmdphysics.h',
  'include/shader.h',
  'include/texthandle.h',
  'include/pmx.h',
  'include/vmd.h',
  'include/motioncontroller.h',
  'include/interpolation.h',
  subdir : 'libmmp')

pkg = import('pkgconfig')
# libmmp is a static library, so its users must also link its dependencies:
# list them as public Requires.  glm is header-only and not every
# distribution ships a glm.pc, so only require it when it was found that way.
pc_requires = ['glfw3', 'epoxy', 'bullet']
if glm_dep.found() and glm_dep.type_name() == 'pkgconfig'
  pc_requires += 'glm'
endif
pkg.generate(
  name : 'libmmp',
  description : 'MikuMikuPenguin: MikuMikuDance PMX/VMD loading, animation and physics',
  libraries : ['-L${libdir}', '-lmmp'],
  subdirs : 'libmmp',
  requires : pc_requires,
  filebase : 'libmmp')

libmmp_dep = declare_dependency(link_with : libmmp,
  include_directories : mmp_inc, dependencies : mmp_deps)

# ---------------------------------------------------------------- pmx viewer
pmx_sources = [
  'src/main.cpp',
  'src/pmxvLogger.cpp',
  'src/viewer.cpp',
  'src/glfw_func_callbacks.cpp',
]
if sound_backend != 'none'
  pmx_sources += 'src/sound.cpp'
else
  pmx_sources += 'src/dummysound.cpp'
endif

executable('pmx', pmx_sources,
  include_directories : include_directories('src'),
  cpp_args : ['-DHAVE_CONFIG_H', '-DDATA_PATH="@0@"'.format(pkgdatadir)],
  dependencies : [libmmp_dep, stb_dep, sound_deps],
  install : true)

# ---------------------------------------------------------------- data
install_data('shaders/model.vert', 'shaders/model.frag',
             'shaders/bulletDebug.vert', 'shaders/bulletDebug.frag',
  install_dir : pkgdatadir / 'shaders')
install_data(
  'data/share/toon01.bmp', 'data/share/toon02.bmp', 'data/share/toon03.bmp',
  'data/share/toon04.bmp', 'data/share/toon05.bmp', 'data/share/toon06.bmp',
  'data/share/toon07.bmp', 'data/share/toon08.bmp', 'data/share/toon09.bmp',
  'data/share/toon10.bmp',
  install_dir : pkgdatadir / 'textures')
install_data('doc/README_EN.html', 'doc/README_JP.html',
  install_dir : pkgdatadir / 'doc')
install_man('man/pmx.1')

# ---------------------------------------------------------------- tests
glm_helper_test = executable('glm_helper_test', 'test/glm_helper_test.cpp',
  dependencies : libmmp_dep)
test('glm_helper', glm_helper_test)

summary({
  'C++ standard' : get_option('cpp_std'),
  'Sound' : sound_backend,
  'Data directory' : pkgdatadir,
}, section : 'MikuMikuPenguin')
