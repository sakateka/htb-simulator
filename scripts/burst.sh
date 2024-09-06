# burst=5555 пакетов в секунду
set 0 rate=5555 burst=5555 thread=1 consume=1
slp 1
mrk 1
dbg 0
set 0 consume=1000 thread=10
slp 0.2
dbg 0
slp 0.3
dbg 0
slp 0.5
mrk 1
slp 1.0
mrk 1
slp 1.0
mrk 1
slp 1.0
mrk 1
dbg 0
slp 10
