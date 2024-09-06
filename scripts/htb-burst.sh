# в этой симуляции 0 tb - это global
# burst глобального tb должен быть равен rate
set 0 rate=2000 burst=10
set 1 rate=500 burst=4000 consume=0 thread=1
set 2 rate=500 burst=4000 consume=100 thread=4
set 3 rate=1000 burst=4000 consume=50 thread=2
set 1 consume=1
# заряжаем burst дочерней квоты
dbg 1
slp 5
dbg 1
slp 5
dbg 1
slp 5
dbg 1
slp 5
dbg 1
set 1 consume=2500 thread=4
slp 1
dbg
slp 1
dbg 1
slp 10
dbg 1
set 1 consume=1
slp 1
dbg
slp 4
