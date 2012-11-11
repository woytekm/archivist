#!/bin/sh

ARCHIVIST_VERSION=`/bin/cat version`
ISCURRENT=`echo $ARCHIVIST_VERSION | grep current | wc -l`

if [ "$ISCURRENT" -eq 1 ]
 then
  CURRENT=`/bin/date +%Y%m%d`
  echo "$ARCHIVIST_VERSION-$CURRENT"
 else
  echo "$ARCHIVIST_VERSION"
 fi

