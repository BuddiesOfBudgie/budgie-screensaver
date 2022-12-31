#!/bin/bash

tx pull -af

pushd po
    rm -f LINGUAS
    for i in *.po ; do
        echo `echo $i|sed 's/.po$//'` >> LINGUAS
    done
popd
