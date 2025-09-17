from osv.modules import api

api.require('libext')
default = api.run("/run_bfs")
