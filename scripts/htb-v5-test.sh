set 0 rate=850 burst=10
set 1 rate=500 burst=5000 consume=1 thread=10
    slp 1
        mrk 1
    dbg 1
set 2 rate=300 burst=6000 consume=90 thread=10
set 3 rate=50 burst=5000 consume=10
    slp 0.2
    dbg 1
    slp 0.3
    dbg 1
    slp 0.5
        mrk 2
    slp 1.0
        mrk 3
set 1 consume=100 # conume=1k = consume*thread
    slp 1.0
        mrk 4
    slp 1.0
        mrk 5
    dbg 1
set 3 consume=1000
    slp 1.0
        mrk 6
    dbg 1
set 2 consume=10 # consume=100
    slp 2
    dbg 1
    slp 2
    dbg 1
    mrk 7
    slp 1
set 2 consume=100
    slp 10
    dbg 1
    mrk 300
    slp 0.1

# ======================= [scenario with 3 threads]
set 1 consume=1 thread=3
set 2 consume=1 thread=3
set 3 consume=1 thread=3
    slp 2
        mrk 1
    dbg 1
set 2 consume=90
set 3 consume=90
    slp 0.2
    dbg 1
    slp 0.3
    dbg 1
    slp 0.5
        mrk 2
    slp 1.0
        mrk 3
set 1 consume=300
    slp 1.0
        mrk 4
    slp 1.0
        mrk 5
    dbg 1
set 3 consume=1
    slp 1.0
        mrk 6
    dbg 1
set 2 consume=10
    slp 2
    dbg 1
    slp 2
set 3 consume=300
    dbg 1
    mrk 7
    slp 1
set 2 consume=100
    slp 10
    dbg 1
    mrk 8
    slp 0.1
