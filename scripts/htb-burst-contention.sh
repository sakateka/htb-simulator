# в этой симуляции 0 tb - это global
# глобальный токен бакет 200rps сумма потребляемых rps не должна пробивать это значение
set 0 rate=200 burst=10 consume=0 thread=1
# дочерние бакеты
set 1 rate=50 burst=4000 consume=10 thread=1 # max real burst 100rps = 10rps(free from tb2) + 90rps (free from tb3)
set 2 rate=50 burst=200 consume=10 thread=4 # total consume 40rps (10rps * 4 threads)
set 3 rate=100 burst=400 consume=5 thread=2 # total consume 10rps
dbg 1
slp 2
dbg 1
slp 2
dbg 1
mrk 2
set 1 consume=250 thread=4 # 4 треда по 250rps = 1000rps
dbg
slp 1
dbg
slp 1
dbg
set 2 consume=30 thread=3 # total consume 50rps(+40 drop or burst) (30rps * 3 threads)
slp 0.2
dbg
slp 0.2
dbg
slp 5
dbg 1
slp 10
dbg 1
set 1 consume=1
slp 10
dbg
slp 10
dbg
slp 10
dbg
slp 10
dbg
slp 1
