#!/bin/bash

source /afs/cern.ch/sw/lcg/external/gcc/4.9.1/x86_64-slc6/setup.sh ""
source /afs/cern.ch/sw/lcg/app/releases/ROOT/5.34.20/x86_64-slc6-gcc48-opt/root/bin/thisroot.sh

if [[ -n "$BOOST_ROOT" ]] ; then
    export BOOST_ROOT="/afs/cern.ch/sw/lcg/external/Boost/1.55.0_python2.7/x86_64-slc6-gcc47-opt"
    
    if [ -n $LD_LIBRARY_PATH ] ; then
        export LD_LIBRARY_PATH=$BOOST_ROOT/lib:$LD_LIBRARY_PATH
    else
        export LD_LIBRARY_PATH=$BOOST_ROOT/lib
    fi
fi

export PATH=`pwd`/bin:$PATH
