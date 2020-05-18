#!/bin/bash

outfile=$1
tmpfile=${outfile}.tmp

version=$(git describe --dirty)
cat >$tmpfile <<EOF
#pragma once
#define GIT_VERSION "$version"
EOF

if [ -r $outfile ]; then
    if diff -q $outfile $tmpfile >/dev/null; then
        # No change to version
        exit 0
    fi
fi

mv $tmpfile $outfile
