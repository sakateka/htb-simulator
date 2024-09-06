set 0 rate=5555 burst=10
set 1 rate=5555 burst=5555 thread=1 consume=1
slp 1
mrk 1
dbg 1
set 1 consume=1000 thread=10
slp 0.2
dbg 1
slp 0.3
dbg 1
slp 0.5
mrk 1
slp 1.0
mrk 1
slp 1.0
mrk 1
slp 1.0
mrk 1
dbg 1
slp 10
