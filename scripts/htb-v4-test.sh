set 0 rate=850 burst=10
set 1 rate=500 burst=5000 consume=1
    slp 1
        mrk 1
    dbg 1
set 2 rate=300 burst=600 consume=900
set 3 rate=50 burst=500 consume=8
    slp 0.2
    dbg 1
    slp 0.3
    dbg 1
    slp 0.5
        mrk 1
    slp 1.0
        mrk 1
set 1 consume=100 thread=10
    slp 1.0
        mrk 1
    slp 1.0
        mrk 1
    dbg 1
set 3 consume=1000 thread=1
    slp 1.0
        mrk 1
    dbg 1
set 2 consume=1000 thread=1
    slp 5
    dbg 1
