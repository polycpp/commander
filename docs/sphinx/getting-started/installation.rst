Installation
============

commander targets C++20 and depends only on the base
`polycpp <https://github.com/enricohuang/polycpp>`_ library.

Tested toolchain matrix for ``v1.0.0``:

- GCC 13+
- Clang 16+
- MSVC 19.44+
- CMake 3.20+

Use from a new project (recommended)
------------------------------------

Start with a minimal ``CMakeLists.txt``:

.. code-block:: cmake

   cmake_minimum_required(VERSION 3.20)
   project(my_app LANGUAGES CXX)
   set(CMAKE_CXX_STANDARD 20)
   set(CMAKE_CXX_STANDARD_REQUIRED ON)

   include(FetchContent)

   FetchContent_Declare(
       polycpp_commander
       GIT_REPOSITORY https://github.com/polycpp/commander.git
       GIT_TAG        v1.0.0
   )
   FetchContent_MakeAvailable(polycpp_commander)

   add_executable(my_app main.cpp)
   target_link_libraries(my_app PRIVATE polycpp::commander)

``polycpp_commander`` fetches ``polycpp`` transitively over HTTPS, so a clean
GitHub checkout works without preconfigured SSH keys.

Build it the usual way:

.. code-block:: bash

   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build

Use :doc:`quickstart` if you want a copy-paste ``main.cpp`` to go with this
setup.

Use local clones during development
-----------------------------------

If you already have commander and polycpp checked out side by side, tell
CMake to use them instead of fetching from GitHub:

.. code-block:: bash

   cmake -B build -G Ninja \
       -DFETCHCONTENT_SOURCE_DIR_POLYCPP=/path/to/polycpp \
       -DFETCHCONTENT_SOURCE_DIR_POLYCPP_COMMANDER=/path/to/commander

If you are building the commander repository itself, use the repo-local switch
instead:

.. code-block:: bash

   cmake -S . -B build -G Ninja -DPOLYCPP_SOURCE_DIR=/path/to/polycpp

That is the path the repository uses for its own test suite.

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

   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
   cmake --build build
   ctest --test-dir build --output-on-failure

All tests should pass on a supported toolchain — if they do not, open an
issue on the `repository <https://github.com/polycpp/commander/issues>`_
with the compiler version and the failing test name.
