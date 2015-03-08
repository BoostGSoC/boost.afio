#!/bin/bash -x
find example -name "*.cpp" | xargs -d '\n' clang-format-3.6 -i
clang-format-3.6 -i include/boost/afio/afio.hpp
#clang-format-3.6 -i include/boost/afio/config.hpp
find include/boost/afio/detail -name "*.hpp" | xargs -d '\n' clang-format-3.6 -i
find include/boost/afio/detail -name "*.ipp" | xargs -d '\n' clang-format-3.6 -i
find test -name "*.hpp" | xargs -d '\n' clang-format-3.6 -i
find test -name "*.cpp" | xargs -d '\n' clang-format-3.6 -i

PWD=$(pwd)
find example -name "*.cpp" | xargs -d '\n' "$PWD/scripts/IndentCmacros.py"
scripts/IndentCmacros.py include/boost/afio/afio.hpp
scripts/IndentCmacros.py include/boost/afio/config.hpp
find include/boost/afio/detail -name "*.hpp" | xargs -d '\n' "$PWD/scripts/IndentCmacros.py"
find include/boost/afio/detail -name "*.ipp" | xargs -d '\n' "$PWD/scripts/IndentCmacros.py"
find test -name "*.hpp" | xargs -d '\n' "$PWD/scripts/IndentCmacros.py"
find test -name "*.cpp" | xargs -d '\n' "$PWD/scripts/IndentCmacros.py"
