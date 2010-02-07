Command line генератор списков для MyHomeLib версии 1.5.

Я пока старался повторить оригинальный алгоритм из LibFileList, без каких либо
извращений с SQL.

Основные возможности (и отличия от уже имеющегося LibFileList):

1.  Работает из командной строки
2.  Не надо ничего устанавливать, никаких WAMP, LAMP и т.д. Используется embedded MySQL.
3.  Результатом работы сразу является INPX файл – не надо ничего переупаковывать
4.  Имена полученных INPX файлов могут включать в себя версию (дату) – ту же, что и внутри
    version.info в INPX, так что можно хранить рядом сколько угодно копий
5.  По умолчанию версия (дата) автоматически берется из дампов Либрусек базы
6.  Умеет производить на свет INPX как при наличии off-line архивов Либрусека, так и просто из
    базы данных Либрусека, без архивов. Вариант без архивов может беть использован для on-line работы
    с Либрусек.
7.  Умеет правильно работать как с "дневными архивами" Либрусека, так и со специально подготовленными
    "большими" архивами
8.  Умеет правильно работать как с FB2 и USR так и со смешанными архивами
9.  Если книжка из архива не найдена в базе умеет прочесть информацию о книге
    из FB2 (случай не регулярно обновляющегося Либрусека)
10. Умеет создавать специальный daily_update.zip чтобы избежать полной перестройки
    базы данных MyHomeLib для дневных архивов Либрусека
11. Не лазит в интернет – в комплект входит командный файл, который умеет закачивать как
    дампы Либрусека, так и ежедневные архивы обновлений.
12. Работает достаточно быстро – полная обработка всех локальных архивов Либрусека и
    дампа сегодняшней базы данных на моем компьютере занимает 1 минуту.
13. Имеются 32 и 64 битные версии

Для запуска наберите lib2inpx.exe в командном окне:

Import file (INPX) preparation tool for MyHomeLib
Version 3.6 (MYSQL 5.1.42)

Usage: lib2inpx.exe [options] <path to SQL dump files>

options:
  --help                Print help message
  --ignore-dump-date    Ignore date in the dump files, use current UTC date 
                        instead
  --clean-when-done     Remove MYSQL database after processing
  --process arg         What to process - "fb2", "usr", "all" (default: fb2)
  --strict arg          What to put in INPX as file type - "ext", "db", 
                        "ignore" (default: ext). ext - use real file extension.
                        db - use file type from database. ignore - ignore files
                        with file extension not equal to file type
  --no-import           Do not import dumps, just check dump time and use 
                        existing database
  --db-name arg         Name of MYSQL database (default: librusec)
  --archives arg        Path(s) to off-line archives. Multiple entries should 
                        be separated by ';'. Each path must be valid and must 
                        point to some archives, or processing would be aborted.
                        (If not present - entire database in converted for 
                        online usage)
  --read-fb2 arg        When archived book is not present in the database - try
                        to parse fb2 in archive to get information. "all" - do 
                        it for all absent books, "last" - only process books 
                        with ids larger than last database id (If not present -
                        no fb2 parsing)
  --inpx arg            Full name of output file (default: <db_name>_<db_dump_d
                        ate>.inpx)
  --comment arg         File name of template (UTF-8) for INPX comment
  --update arg          Starting with "<arg>.zip" produce "daily_update.zip" 
                        (Works only for "fb2")
  --db-format arg       Database format, change date (YYYY-MM-DD). Supported: 
                        2010-02-06. (Default - old librusec format before 
                        2010-02-06)
  --quick-fix           Enforce MyHomeLib database size limits, works with 
                        fix-config parameter. (default: MyHomeLib 1.6.2 
                        constrains)
  --fix-config arg      Allows to specify configuration file with MyHomeLib 
                        database size constrains

Предположим, что сегодняшние дампы Либрусека лежат в уже распакованном
виде в директории d:\librusec\sql (программа подберет все файлы с расширением
.sql), a локальные архивы Либрусека лежат в директории d:\librusec\local
(программа подберет все файлы с расширением .zip). Тогда выдача следующей
команды: “lib2inpx.exe --process fb2 –archives d:\librusec\local d:\librusec\sql”
приведет к тому, что в поддиректории data (в той же директории, откуда запущена
программа) появится файл librusec_20090804.inpx. На экране при этом вы увидите
что-то вроде:

Creating MYSQL database "librusec_20090804"

Importing - "lib.libavtor.sql"        - done in 00:00:01
Importing - "lib.libavtoraliase.sql"  - done in 00:00:00
Importing - "lib.libavtorname.sql"    - done in 00:00:01
Importing - "lib.libbook.sql"         - done in 00:00:09
Importing - "lib.libfilename.sql"     - done in 00:00:00
Importing - "lib.libgenre.sql"        - done in 00:00:01
Importing - "lib.libgenrelist.sql"    - done in 00:00:00
Importing - "lib.libseq.sql"          - done in 00:00:01
Importing - "lib.libseqname.sql"      - done in 00:00:00

Archives processing - 15 file(s) [D:/Books_e/Library/local/]

Processing - "fb2-000024-030559.zip"   - done in 00:00:14 (22807:0:0 records)
Processing - "fb2-030560-060423.zip"   - done in 00:00:16 (24395:0:0 records)
Processing - "fb2-060424-074391.zip"   - done in 00:00:05 (8131:0:0 records)
Processing - "fb2-074392-091839.zip"   - done in 00:00:05 (7134:0:0 records)
Processing - "fb2-091841-104214.zip"   - done in 00:00:05 (7772:0:0 records)
Processing - "fb2-104215-113436.zip"   - done in 00:00:05 (6638:0:0 records)
Processing - "fb2-113437-119690.zip"   - done in 00:00:03 (5508:0:0 records)
Processing - "fb2-119691-132107.zip"   - done in 00:00:04 (6176:0:0 records)
Processing - "fb2-132108-141328.zip"   - done in 00:00:04 (5889:0:0 records)
Processing - "fb2-141329-149815.zip"   - done in 00:00:05 (6161:0:0 records)
Processing - "fb2-149816-153661.zip"   - done in 00:00:02 (3386:0:0 records)
Processing - "fb2-153662-161693.zip"   - done in 00:00:04 (6013:0:0 records)
Processing - "fb2-161694-168102.zip"   - done in 00:00:04 (5441:0:2 records)
Processing - "fb2-168103-173663.zip"   - done in 00:00:03 (4984:0:0 records)
Processing - "fb2-173664-173908.zip"   - done in 00:00:00 (203:0:0 records)

Complete processing took 00:00:59

Если вы хотите создать файл "daily_update.zip", который позволяет добавить
дневные архивы Либрусека к уже имеющейся коллекции без ее полной перестройки,
используйте ключ "update". Выдача команды
“lib2inpx.exe --process fb2 --update 167585-167678 --archives d:\librusec\local d:\librusec\sql”
приведет к созданию daily_update.zip в котором будут содержаться INP для всех имеющихся
в директории дневных архивов от "167585-167678.zip" до более поздних. Архивы с именами,
начинающимися с "fb2-", "usr-", а так же более ранние дневные архивы будут игнорироваться.
Этот режим работает только для FB2 и MyHomeLib версии 1.6 и позже - созданный файл
нужно скопировать в директорию MyHomeLib и инициировать update.

При анализе архивов программа намеренно игнорирует "soft links" (MyHomeLib этого
не делает), что позволяет определенную гибкость при хранении архивов. Например
можно расположить архивы, общие для flibusta и librusec в базовой директории и
хранить дневные обновления, отличающиеся для библиотек отдельно друг от друга,
воспользовавшись "soft links" для создания путей с полными библиотеками для MyHomeLib.
При этом при создании INPX для таких коллекций опция --archives позволяет
перечислить несколько путей к архивам, разделяя их ";". Например:
"--archives=d:\library\local;d:\library\local\librusec"

Выдача “lib2inpx.exe --process usr --archives d:\librusec\local d:\librusec\sql”
произведет на свет librusec_usr_20090804.inpx, в котором соответственно будет
информация по всем файлам, кроме FB2.

Если вы увидели

"Processing - "160443-160588.zip"       - done in 00:00:00 ==> Not in database!"

значит ни одного файла из этого архива не было найдено в процессируемой базe
Либрусека (база старше архивов) или внутри нет ни одного файла подходящего типа
(например FB2 - или не FB2, если вы задавали --process=usr).

Обычно это случается когда Либрусек перестает выкладывать обновленную базу, а дневные
файлы с книгами продолжают появляться. Здесь может помочь ключ "--read-fb2=last". При
этом программа вычислит id самой последней FB2 книги в базе и для всех книг из архивов
с большим id информация для создания inp будет взята из FB2 файла в архиве. Вы увидите
что-то вроде:

Largest FB2 book id in database: 179509

Archives processing - 15 file(s) [D:/Books_e/Library/local/]

Processing - "fb2-000024-030559.zip"   - done in 00:00:14 (22807:0:0 records)
Processing - "fb2-030560-060423.zip"   - done in 00:00:16 (24395:0:0 records)
Processing - "fb2-060424-074391.zip"   - done in 00:00:05 (8131:0:0 records)
Processing - "fb2-074392-091839.zip"   - done in 00:00:05 (7134:0:0 records)
Processing - "fb2-091841-104214.zip"   - done in 00:00:05 (7772:0:0 records)
Processing - "fb2-104215-113436.zip"   - done in 00:00:05 (6638:0:0 records)
Processing - "fb2-113437-119690.zip"   - done in 00:00:03 (5508:0:0 records)
Processing - "fb2-119691-132107.zip"   - done in 00:00:04 (6176:0:0 records)
Processing - "fb2-132108-141328.zip"   - done in 00:00:04 (5889:0:0 records)
Processing - "fb2-141329-149815.zip"   - done in 00:00:05 (6161:0:0 records)
Processing - "fb2-149816-153661.zip"   - done in 00:00:02 (3386:0:0 records)
Processing - "fb2-153662-161693.zip"   - done in 00:00:04 (6013:0:0 records)
Processing - "fb2-161694-168102.zip"   - done in 00:00:04 (5441:0:2 records)
Processing - "fb2-168103-173663.zip"   - done in 00:00:03 (4984:0:0 records)
Processing - "fb2-173664-173908.zip"   - done in 00:00:00 (203:0:0 records)

Archives processing - 12 file(s) [D:/Books_e/Library/local/librusec/]

Processing - "180345-180529.zip"       - done in 00:00:00 (0:131:0 records)
Processing - "180530-180701.zip"       - done in 00:00:00 (0:107:0 records)
Processing - "180702-180996.zip"       - done in 00:00:00 (0:262:0 records)
Processing - "180997-181172.zip"       - done in 00:00:00 (0:135:0 records)
Processing - "181173-181360.zip"       - done in 00:00:00 (0:133:0 records)
Processing - "181361-181497.zip"       - done in 00:00:00 (0:97:0 records)
Processing - "181498-181687.zip"       - done in 00:00:00 (0:171:0 records)
Processing - "181688-181793.zip"       - done in 00:00:00 (0:67:0 records)
Processing - "181794-181963.zip"       - done in 00:00:00 (0:84:1 records)
Processing - "181964-182094.zip"       - done in 00:00:00 (0:85:0 records)
Processing - "182095-182284.zip"       - done in 00:00:00 (0:155:0 records)
Processing - "fb2-173909-180344.zip"   - done in 00:00:03 (4049:561:1 records)

Первый номер в скобках - количество записей, созданное из базы данных, второй - 
количество записей, созданное путем разбора FB2 и третий - количество пропущенных
книг (dummy).

Если указать "--read-fb2=all", то для всех FB2 книг, отсутствующих в базе будет 
сделана попытка прочесть информацию из FB2.

Обратите пожалуйста внимание на то, что некоторые архивы Либрусека в настоящий момент
находятся в странном состоянии. Не FB2 книги внутри них могут находиться в
архивированном состоянии (архивы в архивах). Причем использованы могут быть
разные архиваторы - я встретил RAR и ZIP. По умолчанию (или когда использована
опция --strict=ext) программа положит в INPX реальное расширение имени файла и
проигнорирует поле FileType из базы данных. Если задать --strict=db, то в INPX
будет использоваться поле FileType из базы данных. И, наконец, если использовано
--strict=ignore, тогда lib2inpx не будет включать в INP файлы, для которых
file extension не совпадает с database file type.

При выдаче следующей команды:
”lib2inpx.exe d:\librusec\sql” архивы процессироваться не будут и появившийся
на свет librusec_online_20090804.inpx будет просто содержать все записи из базы
Либрусека (Такой вариант без архивов может быть использован для on-line
работы с Либрусек):

Creating MYSQL database "librusec_20090804"

Importing - "lib.libavtor.sql"        - done in 00:00:01
Importing - "lib.libavtoraliase.sql"  - done in 00:00:00
Importing - "lib.libavtorname.sql"    - done in 00:00:01
Importing - "lib.libbook.sql"         - done in 00:00:09
Importing - "lib.libfilename.sql"     - done in 00:00:01
Importing - "lib.libgenre.sql"        - done in 00:00:01
Importing - "lib.libgenrelist.sql"    - done in 00:00:00
Importing - "lib.libseq.sql"          - done in 00:00:00
Importing - "lib.libseqname.sql"      - done in 00:00:00

Database processing
............................................
132933 records done in 00:00:40

Опция --db-name=flibusta везде заменит "librusec" на "flibusta", т.е. в имени
базы данных, именах произведенных файлов и комментариях в них будет использоваться
другое название.

Опция --db-format=2010-02-06 позволит обработать базы данных с изменившимся после 
5 февраля 2010 года форматом (творчество Librusec).

Ключ --quick-fix обрежет поля, которые при импорте в MyHomeLib не поместятся в базу.
По умолчанию будет использоваться длина из MyHomeLib 1.6.2. Используя --fix-config 
можно специфицировать другие размеры полей. Список поддерживаемых проверок и пример 
находится в файле limits.conf.

Оставшиеся ключи программы не особенно важны: ”clean-when-done” удалит созданную
при работе MYSQL базу данных, “ignore-dump-date” проигнорирует дату в дамп
файлах и использует сегодняшнее число (UTC), а ”inpx” позволит указать имя и
путь желаемого INPX файла. Если нужно запускать программу из batch файла много
раз на одном и том же наборе MYSQL дампов можно воспользоваться ключом
"no-import" и создавать базу данных один раз. В ключе "comment" можно задать путь к
файлу с шаблоном для INPX комментария (UTF-8, единственный "%s" будет заменен на
имя генерируемого INPX файла - см. comment_fb2.utf8 и comment_usr.utf8 в директории
программы). Если этого параметра нет, то комментарии будут генерироваться в формате,
максимально приближенном к тем, что использует MyHomeLib для "стандартных" коллекций
Либрусека.

В предлагаемые архивы в качестве примера входят 2 командных файла: fb2_librusec.cmd
и fb2_flibusta.cmd, которыe принимают параметр – путь к локальным архивам:
“fb2_flibusta.cmd d:\library\local”.  При выполнении каждый скрипт

1. Загрузит недостающие дневные обновления (в d:\library\local\libruec или
   d:\library\local\flibusta)
2. Загрузит сегодняшние дампы базы данных и распакует их
3. Запустит lib2inpx с обработкой архивов – полученный INPX будет лежать в поддиректории data
4. Запустит lib2inpx для всей базы (online вариант) – полученный INPX будет лежать там же

Если указать в командой строке еще один параметр, например:
“fb2_librusec.cmd d:\librusec\local skip” то шаги 1 и 2 будут пропущены.

Вы сможете посмотреть логи закачек в файлах *.log в директории, откуда запущен скрипт.

Осталось создать новую коллекцию в MyHomeLib или запустить "myhomelib.exe /clear",
чтобы начать все заново :)
