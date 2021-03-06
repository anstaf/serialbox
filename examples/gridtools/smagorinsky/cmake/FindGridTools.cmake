# Try to find GridTools libraries and headers.
#
# Usage of this module as follows:
#
#   find_package(GridTools)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#   GRIDTOOLS_ROOT          Set this variable to the root installation of
#                           GridTools if the module has problems finding the
#                           proper installation path.
#
# Variables defined by this module:
#
#   GRIDTOOLS_FOUND         System has GridTools libraries and headers
#   GRIDTOOLS_INCLUDE_DIRS  The location of GridTools headers

find_path(GRIDTOOLS_ROOT NAMES include/gridtools.hpp)

# Look for GridTools headers
find_path(GRIDTOOLS_INCLUDE_DIRS
    NAMES gridtools.hpp
    HINTS ${GRIDTOOLS_ROOT}/include
          $ENV{GRIDTOOLS_ROOT}/include
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GRIDTOOLS DEFAULT_MSG GRIDTOOLS_INCLUDE_DIRS)

if(GRIDTOOLS_FOUND)
    mark_as_advanced(GRIDTOOLS_INCLUDE_DIRS)
else()
    # If the package was required we abort the process
    if(${GridTools_FIND_REQUIRED})
        message(FATAL_ERROR "Could NOT find GRIDTOOLS. (Try setting GRIDTOOLS_ROOT in the env)")
    endif(${GridTools_FIND_REQUIRED})
endif()

