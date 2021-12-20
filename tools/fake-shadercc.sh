#!/bin/sh
#
# Github LFS has stupidly low limits.  Fortunately for unit tests we don't need a shader compiler,
# just something that generates a file like glslc.
#
#  fake-shadercc.sh -o ${OUT} ${IN}

while getopts o: flag
do
    case "${flag}" in
        o) outfilename=${OPTARG};;
    esac
done
shift $((OPTIND-1))
infilename=$1

echo Fake shader Compiler $infilename to $oufilename
cat $infilename > $outfilename

