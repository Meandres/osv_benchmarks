from osv.modules import api

default = api.run('/ycsb -load -run -db leveldb -P workloads/workloadb -P leveldb/leveldb.properties -p threadcount=4 -p recordcount=10000000 -p leveldb.cache_size=134217728 -s')
