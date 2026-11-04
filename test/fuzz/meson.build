# libFuzzer targets for the PMX, PMD and VMD loaders (meson -Dfuzz=true,
# with clang).  libmmp is rebuilt here with AddressSanitizer,
# UndefinedBehaviorSanitizer and libFuzzer coverage; see README.md.
if cxx.get_id() != 'clang'
  error('-Dfuzz=true needs clang (e.g. CXX=clang++ meson setup build-fuzz -Dfuzz=true)')
endif

fuzz_sanitizers = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                   '-fno-omit-frame-pointer']
libmmp_fuzz = static_library('mmp_fuzz', libmmp_sources,
  include_directories : mmp_inc,
  dependencies : mmp_deps,
  cpp_args : fuzz_sanitizers + ['-fsanitize=fuzzer-no-link'])

foreach target : ['pmx', 'pmd', 'vmd']
  executable('fuzz_' + target, 'fuzz_' + target + '.cpp', 'fuzz_common.cpp',
    include_directories : mmp_inc,
    dependencies : mmp_deps,
    link_with : libmmp_fuzz,
    cpp_args : fuzz_sanitizers + ['-fsanitize=fuzzer'],
    link_args : fuzz_sanitizers + ['-fsanitize=fuzzer'])
endforeach
