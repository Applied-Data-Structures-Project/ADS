# Course rule: no STL data structures in our own code (include/ and src/).
# std::string and STL used inside third-party libraries (Crow) are fine.
# Usage (done automatically by CTest):
#   cmake -DSCAN_DIRS="include;src" -P tests/no_stl_containers.cmake

set(FORBIDDEN_HEADERS
  vector list forward_list deque array
  map unordered_map set unordered_set
  queue stack bitset span)

set(violations "")
foreach(dir ${SCAN_DIRS})
  file(GLOB_RECURSE files "${dir}/*.hpp" "${dir}/*.h" "${dir}/*.cpp")
  foreach(f ${files})
    file(STRINGS "${f}" lines REGEX "^[ \t]*#[ \t]*include[ \t]*<")
    foreach(line ${lines})
      foreach(h ${FORBIDDEN_HEADERS})
        if(line MATCHES "<${h}>")
          list(APPEND violations "${f}: ${line}")
        endif()
      endforeach()
    endforeach()
  endforeach()
endforeach()

if(violations)
  list(JOIN violations "\n  " msg)
  message(FATAL_ERROR "STL container headers are not allowed:\n  ${msg}")
endif()
message(STATUS "No STL container headers found.")

