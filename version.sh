#!/bin/sh

VERSION=`/bin/cat version`
CURRENT=`/bin/date +%Y%m%d`

echo "$VERSION-$CURRENT"
