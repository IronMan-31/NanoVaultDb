#!/usr/bin/env bash
echo "Cleaning old builds..."
rm -rf dist/* build/* wheelhouse/* nanovaultdb.egg-info
echo "Building raw python wheel..."
CC=gcc-13 CXX=g++-13 python3 setup.py bdist_wheel
auditwheel repair dist/*.whl
