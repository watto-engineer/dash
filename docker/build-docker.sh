#!/usr/bin/env bash

export LC_ALL=C

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR/.. || exit

DOCKER_IMAGE=${DOCKER_IMAGE:-wagerr/wagerrd-develop}
DOCKER_TAG=${DOCKER_TAG:-latest}

BUILD_DIR=${BUILD_DIR:-.}

rm docker/bin/*
mkdir docker/bin
cp $BUILD_DIR/src/wagerrd docker/bin/
cp $BUILD_DIR/src/wagerr-cli docker/bin/
cp $BUILD_DIR/src/wagerr-tx docker/bin/
strip docker/bin/wagerrd
strip docker/bin/wagerr-cli
strip docker/bin/wagerr-tx

docker build --pull -t $DOCKER_IMAGE:$DOCKER_TAG -f docker/Dockerfile docker
