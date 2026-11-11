# tests
Every tests/*.cpp becomes its own CTest executable (pass = main returns 0).
no_stl_containers.cmake fails the build's tests if include/ or src/ includes an STL container header.
Run: ctest --test-dir build --output-on-failure
