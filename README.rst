=====================================
Utility to remux Ogg Vorbis into FSB5
=====================================

This project allows you to remux `Ogg <https://xiph.org/ogg/>`_ `Vorbis <https://xiph.org/vorbis/>`_
into FSB5, the file format used by `FMOD <https://www.fmod.com/>`_, without re-encoding,
avoiding `generation loss <https://en.wikipedia.org/wiki/Generation_loss>`_ and allowing you to use other encoders such as
`aoTuV <https://ao-yumi.github.io/aotuv_web/>`_ or recompressors like `rehuff <https://github.com/uyjulian/rehuff>`_.

Additionally, this project allows you to specify beginning and loop points, which can be useful for seemlessly looping audio.

How to use
==========

First, please download the
`macOS <https://github.com/uyjulian/oggvorbis2fsb5/releases/download/latest/oggvorbis2fsb5-macos.zip>`_,
`Ubuntu <https://github.com/uyjulian/oggvorbis2fsb5/releases/download/latest/oggvorbis2fsb5-ubuntu.zip>`_, or
`Win32 <https://github.com/uyjulian/oggvorbis2fsb5/releases/download/latest/oggvorbis2fsb5-win32.zip>`_
build.

This utility is used from the command line:: bash

    /path/to/oggvorbis2fsb5 /path/to/input/file.ogg /path/to/output/file.fsb 33333 44444

The arguments ``33333`` and ``44444`` are optional and specify the loop start and end points.
If they are omitted, the loop metadata will not be written into the FSB5 file.

How to build
============

This project can be built by using the Meson build system.  
For more information about the system, please visit the following location: https://mesonbuild.com/  

Meson toolchain files can be used to cross compile to different platforms, such as when using `mingw w64 <http://mingw-w64.org/doku.php>`_.  
For your convenience, Meson toolchain files are located here: https://github.com/krkrsdl2/meson_toolchains  

License
=======

This project is licensed under the MIT license. Please read the ``LICENSE`` file for more information.
