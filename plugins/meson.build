lib_args = common_args + []

# sources used to compile this plug-in
plugin_sources = [
  'gstyuvconvert.c',
  'gstyuvconvert.h'
]

#add_global_arguments('-I/usr/local/include', language : 'c')
#add_global_link_arguments('-L/usr/local/lib', language : 'c')
#add_global_link_arguments('-lyuv', language : 'c')

gst_video_dep = dependency('gstreamer-video-1.0')
libyuv_proj = subproject('libyuv')
libyuv_dep = libyuv_proj.get_variable('gst_libyuv_dep')

install_dir = join_paths(gst_video_dep.get_pkgconfig_variable('libdir'), 'gstreamer-1.0')

shlib = shared_library('gstyuvconvert',
  plugin_sources,
  c_args : lib_args,
  include_directories: [configinc],
  dependencies : [ plugin_deps, gst_video_dep, libyuv_dep ],
  gnu_symbol_visibility : 'hidden',
  install : true,
  install_dir : install_dir,
)

# Make this library usable as a Meson subproject.
gst_yuvconvert_dep = declare_dependency(
  include_directories: include_directories('.'),
  link_with : shlib)

pkg_mod = import('pkgconfig')
pkg_mod.generate(
  name : 'gst-yuvconvert',
  filebase : 'gst-yuvconvert',
  description : 'Meson sample project.',
  subdirs : 'src',
  libraries : shlib,
  version : '"@0@"'.format(meson.project_version()),
)
