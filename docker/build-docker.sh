#!/usr/bin/env bash

export LC_ALL=C

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR/.. || exit

DOCKER_IMAGE=${DOCKER_IMAGE:-bytzcurrency/bytzd-develop}
DOCKER_TAG=${DOCKER_TAG:-latest}

BUILD_DIR=${BUILD_DIR:-.}

rm docker/bin/*
mkdir docker/bin
cp $BUILD_DIR/src/bytzd docker/bin/
cp $BUILD_DIR/src/bytz-cli docker/bin/
cp $BUILD_DIR/src/bytz-tx docker/bin/
strip docker/bin/bytzd
strip docker/bin/bytz-cli
strip docker/bin/bytz-tx

docker build --pull -t $DOCKER_IMAGE:$DOCKER_TAG -f docker/Dockerfile docker
