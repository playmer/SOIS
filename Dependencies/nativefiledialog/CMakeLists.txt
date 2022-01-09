add_library(nativefiledialog STATIC)

target_sources(nativefiledialog 
PRIVATE
    repo/src/include/nfd.h
    repo/src/common.h
    repo/src/nfd_common.c
    repo/src/nfd_common.h
)

if (ANDROID)

elseif (WIN32)
    target_sources(nativefiledialog 
    PRIVATE
        repo/src/nfd_win.cpp
    )
elseif(APPLE) # Defend against iOS? Why are you running this on iOS? I guess you might be cross compiling....
    target_sources(nativefiledialog 
    PRIVATE
        repo/src/nfd_cocoa.m
    )
elseif(UNIX) # Probably should defend against Cygwin here but....
    target_sources(nativefiledialog 
    PRIVATE
        repo/src/nfd_gtk.c
    )
endif()

target_include_directories(nativefiledialog 
PUBLIC 
    repo/src/include
)
