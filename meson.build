project(
	'oggvorbis2fsb5',
	[
		'c',
	],
	default_options: [
		'optimization=3',
		'debug=true',
		'warning_level=1'
	],
	version: 'unknown'
)

oggvorbis2fsb5_cflags = []

oggvorbis2fsb5_ldflags = []
if target_machine.system() == 'windows'
	oggvorbis2fsb5_ldflags += [
		'-static',
	]
endif
if target_machine.system() == 'linux'
	oggvorbis2fsb5_ldflags += [
		'-lm',
	]
endif

oggvorbis2fsb5_src = [
	'oggvorbis2fsb5.c',
	'external/ogg/src/bitwise.c',
	'external/ogg/src/framing.c',
	'external/vorbis/lib/bitrate.c',
	'external/vorbis/lib/block.c',
	'external/vorbis/lib/codebook.c',
	'external/vorbis/lib/envelope.c',
	'external/vorbis/lib/floor0.c',
	'external/vorbis/lib/floor1.c',
	'external/vorbis/lib/info.c',
	'external/vorbis/lib/lpc.c',
	'external/vorbis/lib/lsp.c',
	'external/vorbis/lib/mapping0.c',
	'external/vorbis/lib/mdct.c',
	'external/vorbis/lib/psy.c',
	'external/vorbis/lib/registry.c',
	'external/vorbis/lib/res0.c',
	'external/vorbis/lib/sharedbook.c',
	'external/vorbis/lib/smallft.c',
	'external/vorbis/lib/synthesis.c',
	'external/vorbis/lib/vorbisfile.c',
	'external/vorbis/lib/window.c',
]

oggvorbis2fsb5_includes = [
	'external/ogg/include',
	'external/vorbis/include',
]

executable(
	'oggvorbis2fsb5',
	sources: oggvorbis2fsb5_src,
	c_args: oggvorbis2fsb5_cflags,
	link_args: oggvorbis2fsb5_ldflags,
	include_directories: include_directories(oggvorbis2fsb5_includes),
)
