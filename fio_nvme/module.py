from osv.modules import api

#api.require('libz')
default = api.run('/fio /osv_nvme_test.fio')
