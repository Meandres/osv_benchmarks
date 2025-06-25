from osv.modules import api

default = api.run('/ycsb -load -run -db leveldb -P workloads/workloadb -P leveldb/leveldb.properties -s')
