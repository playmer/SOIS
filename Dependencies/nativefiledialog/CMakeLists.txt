add_library(nativefiledialog STATIC)

target_sources(nativefiledialog 
PRIVATE
    repo/src/include/nfd.h
    repo/src/common.h
    repo/src/nfd_common.c
    repo/src/nfd_common.h
)

if (WIN32)
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

    find_package(GTK2 REQUIRED)
    target_include_directories(nativefiledialog PRIVATE ${GTK2_INCLUDE_DIRS})
    target_link_libraries(nativefiledialog PRIVATE ${GTK2_LIBRARIES})
endif()

target_include_directories(nativefiledialog 
PUBLIC 
    repo/src/include
)
