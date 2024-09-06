# в этой симуляции 0 tb - это global
# глобальный токен
# бакет 200rps сумма потребляемых rps не должна пробивать это значение
set 0 rate=200 burst=1 consume=0 thread=1
#  дочерние бакеты
set 1 rate=50 burst=600 consume=10 thread=1
set 2 rate=50 burst=600 consume=10 thread=4
set 3 rate=100 burst=400 consume=5 thread=10
slp 0.5
dbg 1
mrk 200
set 1 consume=100 thread=1 # должен быть burst но его часть срежется дропами
dbg
slp 1
mrk 10
dbg
slp 1
mrk 2
dbg
slp 1
mrk 2
dbg
set 2 consume=30 thread=3 # total consume 50rps(+40 drop or burst) (30rps * 3 threads)
slp 0.5
dbg
slp 0.5
mrk 2
dbg
slp 5
dbg 1
set 3 consume=30 thread=4
dbg
slp 0.2
dbg 1
slp 10
mrk 1
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
