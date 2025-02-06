from osv.modules import api
from osv.modules.filemap import FileMap

usr_files = FileMap()

# libduckdb.so is dynamically linked from src/
# FIXME: Build duckdb statically linked and remove .include(src/**)
usr_files.add('${OSV_BASE}/benchmarks/duckdb/duckdb/build/release').to('/ddb') \
        .include('benchmark/**') \
        .include('src/**') \
        .include('duckdb')

# benchmark_runner expects the benchmarks to be found at DUCKDB_ROOT_DIRECTORY,
# we set DUCKDB_ROOT_DIRECTORY to / and mount benchmarks there
usr_files.add('${OSV_BASE}/benchmarks/duckdb/duckdb/benchmark').to('/benchmark')

default = api.run('/ddb/duckdb')
