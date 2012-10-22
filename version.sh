#!/bin/sh

ARCHIVIST_VERSION=`/bin/cat version`
CURRENT=`/bin/date +%Y%m%d`

echo "$ARCHIVIST_VERSION-$CURRENT"
