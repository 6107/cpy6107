# ------------------------------------------------------------------------ #
#      o-o      o                o                                         #
#     /         |                |                                         #
#    O     o  o O-o  o-o o-o     |  oo o--o o-o o-o                        #
#     \    |  | |  | |-' |   \   o | | |  |  /   /                         #
#      o-o o--O o-o  o-o o    o-o  o-o-o--O o-o o-o                        #
#             |                           |                                #
#          o--o                        o--o                                #
#                        o--o      o         o                             #
#                        |   |     |         |  o                          #
#                        O-Oo  o-o O-o  o-o -o-    o-o o-o                 #
#                        |  \  | | |  | | |  |  | |     \                  #
#                        o   o o-o o-o  o-o  o  |  o-o o-o                 #
#                                                                          #
#    Jemison High School - Huntsville Alabama                              #
# ------------------------------------------------------------------------ #
# Build/lint/test environment with glibc >= 2.41, which WPILib 2027 alpha-7+ needs
# (robotpy-native-wpihal is tagged manylinux_2_41; Ubuntu 24.04 has glibc 2.39).
#
#   docker build -f docker/Dockerfile --build-arg WPILIB_VERSION=2027.0.0a8 -t cpy6107-dev:a8 .
# Normally used through docker/run.sh.
ARG PYTHON_IMAGE=python:3.14-trixie
FROM ${PYTHON_IMAGE}

ARG WPILIB_VERSION=2027.0.0a8
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      g++-14 clang-19 cmake ninja-build git ca-certificates \
 && rm -rf /var/lib/apt/lists/*

# clang-format/clang-tidy come from pip so the versions match the host .venv and CI.
RUN python -m venv /opt/venv \
 && /opt/venv/bin/pip install --no-cache-dir --pre \
      "wpilib==${WPILIB_VERSION}" "robotpy-commands-v2==${WPILIB_VERSION}" nanobind scikit-build-core pytest pytest-cov gcovr \
      ruff mypy "clang-format==19.1.7" "clang-tidy==19.1.0.1" \
      build auditwheel patchelf twine

ENV PATH=/opt/venv/bin:$PATH \
    CPY6107_PYTHON=/opt/venv/bin/python \
    CPY6107_WPILIB_VERSION=${WPILIB_VERSION} \
    HOME=/tmp

WORKDIR /work
CMD ["cmake", "--workflow", "--preset", "linux-ci"]
