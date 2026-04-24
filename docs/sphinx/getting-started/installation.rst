Installation
============

commander targets C++20 and builds with clang ≥ 14 or gcc ≥ 11. It depends
only on the base `polycpp <https://github.com/enricohuang/polycpp>`_ library
(and none).

CMake FetchContent (recommended)
--------------------------------

Add the library to your ``CMakeLists.txt``:

.. code-block:: cmake

   include(FetchContent)

   FetchContent_Declare(
       polycpp_commander
       GIT_REPOSITORY https://github.com/polycpp/commander.git
       GIT_TAG        master
   )
   FetchContent_MakeAvailable(polycpp_commander)

   add_executable(my_app main.cpp)
   target_link_libraries(my_app PRIVATE polycpp::commander)

The first configure pulls ``polycpp`` transitively, so the build tree may be
large. Pin ``GIT_TAG`` to a specific commit for reproducible builds.

Using a local clone
-------------------

If you already have commander and polycpp checked out side by side, tell
CMake to use them instead of fetching from GitHub:

.. code-block:: bash

   cmake -B build -G Ninja \
       -DFETCHCONTENT_SOURCE_DIR_POLYCPP=/path/to/polycpp \
       -DFETCHCONTENT_SOURCE_DIR_POLYCPP_COMMANDER=/path/to/commander

This is the path CI uses for the test suite — see ``tests/`` in the repo.

Build options
-------------

``POLYCPP_COMMANDER_BUILD_TESTS``
    Build the GoogleTest suite. Defaults to ``ON`` for standalone builds and
    ``OFF`` when consumed via FetchContent.

``POLYCPP_IO``
    ``asio`` (default) or ``libuv`` — inherited from polycpp.

``POLYCPP_SSL_BACKEND``
    ``boringssl`` (default) or ``openssl``.

``POLYCPP_UNICODE``
    ``icu`` (recommended) or ``builtin``. ICU enables the Intl surface that
    several polycpp headers pull into their public signatures.

Verifying the install
---------------------

.. code-block:: bash

   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
   cmake --build build
   ctest --test-dir build --output-on-failure

All tests should pass on a supported toolchain — if they do not, open an
issue on the `repository <https://github.com/polycpp/commander/issues>`_
with the compiler version and the failing test name.
