#!/bin/sh
#
# Regenerate the Mapbox Vector Tile test reference outputs.
#
# Use this after any change to the MVT encoder (MapboxVectorTile.cpp). It runs
# the plugin IN-PROCESS via PluginTest against the local test data and copies
# the freshly produced output over the committed references. It does NOT talk to
# any external server (e.g. back2) -- the plugin under test is the freshly built
# local wms.so. It needs the same environment as `make test`: the geonames test
# database and redis.
#
# The .get reference files store raw protobuf TextFormat, so a self-consistent
# but spec-wrong geometry encoding cannot be caught by byte comparison alone --
# always eyeball the diff (and rely on unit/test_mvt_geometry) after regenerating.

set -e
cd "$(dirname "$0")"

MVT_TESTS="tiles_gettile_mvt_isoband.get \
           tiles_gettile_mvt_isoline.get \
           tiles_gettile_mvt_circles.get \
           tiles_gettile_mvt_numbers.get"

echo "*** Relinking the plugin so PluginTest loads the current encoder..."
make -C .. wms.so

echo "*** Preparing the test environment (configs, redis, geonames)..."
make templates cnf/authentication.conf cnf/geonames.conf cnf/gis.conf \
     cnf/avi.conf cnf/observation.conf start-redis-db start-geonames-db

echo "*** Running the MVT tests; regenerated output lands in failures/..."
rm -f $(for t in $MVT_TESTS; do echo "failures/$t"; done)
INPUTS=$(for t in $MVT_TESTS; do echo "input/$t"; done)
./TestRunner.sh $INPUTS || true

echo "*** Updating references from regenerated output..."
for t in $MVT_TESTS; do
  if [ -f "failures/$t" ]; then
    cp "failures/$t" "output/$t"
    echo "    updated output/$t"
  else
    echo "    output/$t unchanged"
  fi
done

echo "*** Stopping the test environment..."
make stop-redis-db stop-geonames-db

echo "*** Done. Review 'git diff' on test/output/tiles_gettile_mvt_*.get"
