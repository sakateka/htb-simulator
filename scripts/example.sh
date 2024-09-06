 # это не баш скрипт, это набор команд симулятору, поддерживаемые команды
# можно посмотреть так ./simulator <<<"help" или интерактивно в симуляторе
# на момент написания этого коммента поддержано 4 команды set slp dbg mrk
set 0 consume=0 thread=1   # set NB rate=Nrps burst=Nrps consume=Nrps thread=N
                           # NB - это номер бакета (от нуля)
                           # Nrps - это целое число - количество запросов в секунду
                           # rate - это лимит в rps заданный для бакета
                           # burst - разрешённый burst в кол-ве запросов в секунду
                           # consume - скорость с которой один тред потребляет ресурс tb
                           # thread=N - это кол-во тредов делающих симуляцию
                           #            запросов в токен бакет

# BEWARE если поставить consume=N (где N > 0) до любых манипуляций с burst, rate
# то tb->value может зарядиться каким-то значением токенов и далее может
# вылиться в неожиданный пик использования выше квоты, это нужно учитывать при симуляции
# и ставить первый consume > 0 только после полной желаемой настройки бакета.
slp 1.1                    # slp N - спать N секунд, можно указывать
set 0 rate=10 burst=10     # аргументы set могут идти в любом порядке и любым набором
set 0 thread=3
set 0 consume=20
dbg 0                      # dbg NB - вывести состояние бакета NB
slp 5
set 0 consume=2
slp 2
set 0 consume=4
slp 2
set 0 consume=6
slp 2
set 0 consume=8
slp 2
set 0 consume=10
slp 2
set 0 consume=12
slp 2
set 0 consume=14
slp 2
set 0 consume=16
slp 2
set 0 consume=18
slp 2
set 0 consume=20
slp 5
set 0 consume=0
dbg 0
slp 5
