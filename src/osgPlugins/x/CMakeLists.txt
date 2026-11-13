SET(TARGET_SRC
    ReaderWriterDirectX.cpp
    directx.cpp
    mesh.cpp
    parser.cpp )
SET(TARGET_H
    directx.h
    mesh.h
    parser.h
    types.h )

# zlib is needed for compressed ('tzip'/'bzip') .x files
IF(ZLIB_FOUND)
    ADD_DEFINITIONS(-DUSE_ZLIB)
    INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIR})
    SET(TARGET_LIBRARIES_VARS ZLIB_LIBRARIES)
ENDIF()

#### end var setup  ###
SETUP_PLUGIN(x)
