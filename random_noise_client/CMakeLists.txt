check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

build_lib(
    LIBNAME random_noise_client
    SOURCE_FILES model/random_noise_client.cc
                 helper/random_noise_client_helper.cc
    HEADER_FILES model/random_noise_client.h
                 helper/random_noise_client_helper.h
    LIBRARIES_TO_LINK ${libcore}
)
