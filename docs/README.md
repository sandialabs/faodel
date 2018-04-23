FAODEL Documentation
====================

FAODEL uses Doxygen for code documentation. The faodel/docs directory
ships empty because we expect users to generate the docs themselves. There
are two ways to generate the docs:

Add 'docs' to the target list during the build process
------------------------------------------------------
Most users will simply add 'doc' to the build target list. For example:

    cd faodel/build
    cmake ..
    make doc install


Build docs manually
-------------------
It is also possible to manually build the docs by running the doxygen
command directly. The Doxyfile has relative include paths that assumer
the operation was started in FAODEL root directory. Therefore, to
manually generate the docs, do the following:

    cd faodel
    doxygen docs/Doxyfile


