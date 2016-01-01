Command line генератор списков для MyHomeLib версии 1.5 и позже.

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
Version 5.51 (MYSQL 5.5.17)

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
  --prefer-fb2 arg      Try to parse fb2 in archive to get information 
                        (default: ignore). "ignore" - ignore fb2 information, 
                        "merge" - always prefer book sequence info from fb2, 
                        "replace" - always use book sequence info from fb2
  --sequence arg        How to process sequence types from database (default: 
                        author). "author" - always select author's book 
                        sequence, "publisher" - always select publisher's book 
                        sequence, "ignore" - don't do any processing. Only 
                        relevant for librusec database format 2011-11-06
  --inpx arg            Full name of output file (default: 
                        <db_name>_<db_dump_date>.inpx)
  --comment arg         File name of template (UTF-8) for INPX comment
  --update arg          Starting with "<arg>.zip" produce "daily_update.zip" 
                        (Works only for "fb2")
  --db-format arg       Database format, change date (YYYY-MM-DD). Supported: 
                        2010-02-06, 2010-03-17, 2010-04-11, 2011-11-06. 
                        (Default - old librusec format before 2010-02-06)
  --clean-authors       Clean duplicate authors (librusec)
  --clean-aliases       Fix libavtoraliase table (flibusta)
  --follow-links        Do not ignore symbolic links
  --inpx-format arg     INPX format, Supported: 1.x, 2.x, (Default - old 
                        MyHomeLib format 1.x)
  --quick-fix           Enforce MyHomeLib database size limits, works with 
                        fix-config parameter. (default: MyHomeLib 1.6.2 
                        constrains)
  --fix-config arg      Allows to specify configuration file with MyHomeLib 
                        database size constrains
  --verbose             More output... (default: off)

Предположим, что сегодняшние дампы Либрусека лежат в уже распакованном
виде в директории d:\librusec\sql (программа подберет все файлы с расширением
.sql), a локальные архивы Либрусека лежат в директории d:\librusec\local
(программа подберет все файлы с расширением .zip). Тогда выдача следующей
команды: “lib2inpx.exe --process fb2 –archives d:\librusec\local d:\librusec\sql”
приведет к тому, что в поддиректории data (в той же директории, откуда запущена
программа) появится файл librusec_20110514.inpx. На экране при этом вы увидите
что-то вроде:

Creating MYSQL database "librusec_20110514"

Importing - "libavtor.sql"            - done in 00:00:02
Importing - "libavtoraliase.sql"      - done in 00:00:01
Importing - "libavtorname.sql"        - done in 00:00:01
Importing - "libbook.sql"             - done in 00:00:18
Importing - "libgenre.sql"            - done in 00:00:01
Importing - "libgenrelist.sql"        - done in 00:00:00
Importing - "libgenremeta.sql"        - done in 00:00:01
Importing - "libjoinedbooks.sql"      - done in 00:00:00
Importing - "libquality.sql"          - done in 00:00:00
Importing - "librate.sql"             - done in 00:00:06
Importing - "libseq.sql"              - done in 00:00:00
Importing - "libseqname.sql"          - done in 00:00:00
Importing - "libsrclang.sql"          - done in 00:00:00

Processing duplicate authors
*  De-duping author   103059 (   0) : BB Brunes--
*  De-duping author   103061 (   0) : BB Brunes--
*  De-duping author   102717 (   0) : Pferd im Mantel--
*  De-duping author   102719 (   0) : Pferd im Mantel--
*  De-duping author   102721 (   0) : Pferd im Mantel--
*  De-duping author   102723 (   0) : Pferd im Mantel--
*  De-duping author   102725 (   0) : Pferd im Mantel--
*  De-duping author   103163 (   0) : Лещинская--
*  De-duping author   103165 (   0) : Лещинская--
*  De-duping author   103143 (   0) : СМинкин--
*  De-duping author   103145 (   0) : СМинкин--
*  De-duping author   102685 (   0) : Menden-A-J
*  De-duping author   102687 (   0) : Menden-A-J
*  De-duping author   102689 (   0) : Menden-A-J
*  De-duping author   102797 (   0) : Moh,acsi-Enik,o-
*  De-duping author   102799 (   0) : Moh,acsi-Enik,o-
*  De-duping author   102801 (   0) : Moh,acsi-Enik,o-
*  De-duping author   102803 (   0) : Hern,andez-F,elix-
*  De-duping author   102805 (   0) : Hern,andez-F,elix-
*  De-duping author   102793 (   0) : Habony-G,abor-
*  De-duping author   102795 (   0) : Habony-G,abor-
*  De-duping author   102695 (   0) : S,arkozy-Gyula-
*  De-duping author   102697 (   0) : S,arkozy-Gyula-
*  De-duping author   102699 (   0) : S,arkozy-Gyula-
*  De-duping author   102701 (   0) : S,arkozy-Gyula-
*  De-duping author   103035 (   0) : Никифоров-H-В
*  De-duping author   103037 (   0) : Никифоров-H-В
*  De-duping author   103115 (   0) : nski-Krzysztof-
*  De-duping author   103117 (   0) : nski-Krzysztof-
*  De-duping author   102633 (   0) : 'Donohoe-Nick-
*  De-duping author   102635 (   0) : 'Donohoe-Nick-
*  De-duping author   103119 (   0) : Закарьян-P-
*  De-duping author   103121 (   0) : Закарьян-P-
*  De-duping author   102679 (   0) : 'Shea-Patti-
*  De-duping author   102681 (   0) : 'Shea-Patti-
*  De-duping author   102683 (   0) : 'Shea-Patti-
*  De-duping author   102703 (   0) : Dymczyk-S,lawomir-
*  De-duping author   102705 (   0) : Dymczyk-S,lawomir-
*  De-duping author   102707 (   0) : Dymczyk-S,lawomir-
*  De-duping author   102709 (   0) : Dymczyk-S,lawomir-
*  De-duping author   103067 (   0) : Litkevich-Yuriy-
*  De-duping author   103069 (   0) : Litkevich-Yuriy-
*  De-duping author   102691 (   0) : Kr,olicki-Zbigniew-Andrzej
*  De-duping author   102693 (   0) : Kr,olicki-Zbigniew-Andrzej
*  De-duping author   102669 (   0) : lowanow-law-
*  De-duping author   102671 (   0) : lowanow-law-
*  De-duping author   102653 (   0) : Йовик (Соколенко)-?ван-
*  De-duping author   102655 (   0) : Йовик (Соколенко)-?ван-
*  De-duping author   103011 (   0) : Граната-А-
*  De-duping author   103013 (   0) : Граната-А-
*  De-duping author   103151 (   0) : Малаховой-А-
*  De-duping author   103153 (   0) : Малаховой-А-
*  De-duping author   103043 (   0) : Елистратова-А-А
*  De-duping author   103045 (   0) : Елистратова-А-А
*  De-duping author   103135 (   0) : Левинтон-А-Г
*  De-duping author   103137 (   0) : Левинтон-А-Г
*  De-duping author   103187 (   0) : Филиппов-А-Ф
*  De-duping author   103189 (   0) : Филиппов-А-Ф
*  De-duping author   103159 (   0) : К,о,г,а,н-А,н,а,т,о,л,и,й-
*  De-duping author   103161 (   0) : К,о,г,а,н-А,н,а,т,о,л,и,й-
*  De-duping author   102711 (   0) : Н-Абаев-В
*  De-duping author   102713 (   0) : Н-Абаев-В
*  De-duping author   102715 (   0) : Н-Абаев-В
*  De-duping author   103107 (   0) : Вулых-Александр-
*  De-duping author   103109 (   0) : Вулых-Александр-
*  De-duping author   102963 (   0) : Каневский-Александр-
*  De-duping author   102965 (   0) : Каневский-Александр-
*  De-duping author   103131 (   0) : Самсонов-Александр-
*  De-duping author   103133 (   0) : Самсонов-Александр-
   De-duping author   103685 (   0) : Сульянов-Анатолий-Константинович
*  De-duping author   102987 (   0) : Ланьков-Андрей-
*  De-duping author   102989 (   0) : Ланьков-Андрей-
*  De-duping author   103167 (   0) : Ливадный-Андрей-Ливадный
*  De-duping author   103169 (   0) : Ливадный-Андрей-Ливадный
*  De-duping author   102967 (   0) : Магдалина-Анна-
*  De-duping author   102969 (   0) : Магдалина-Анна-
*  De-duping author   102979 (   0) : Марти-Беверли-
*  De-duping author   102981 (   0) : Марти-Беверли-
*  De-duping author   103197 (   0) : Тараканов-Борис-
*  De-duping author   103199 (   0) : Тараканов-Борис-
*  De-duping author   102975 (   0) : Федоров-Борис-
*  De-duping author   102977 (   0) : Федоров-Борис-
*  De-duping author   103071 (   0) : Быстрое-В-
*  De-duping author   103073 (   0) : Быстрое-В-
*  De-duping author   103075 (   0) : Макеев-В-
*  De-duping author   103077 (   0) : Макеев-В-
   De-duping author   103521 (   0) : Челноковой-В-
*  De-duping author   102945 (   0) : Захаренков-В-М
*  De-duping author   102947 (   0) : Захаренков-В-М
*  De-duping author   103007 (   0) : Орел-В-Э
*  De-duping author   103009 (   0) : Орел-В-Э
*  De-duping author   103103 (   0) : Степанцов-Вадим-
*  De-duping author   103105 (   0) : Степанцов-Вадим-
*  De-duping author   102641 (   0) : Волковинський-Валер?й-
*  De-duping author   102643 (   0) : Волковинський-Валер?й-
*  De-duping author   102645 (   0) : Волковинський-Валер?й-
*  De-duping author   102983 (   0) : Ерохин-Валерий-
*  De-duping author   102985 (   0) : Ерохин-Валерий-
*  De-duping author   102727 (   0) : Шегелов-Валерий-Николаевич
*  De-duping author   102729 (   0) : Шегелов-Валерий-Николаевич
*  De-duping author   102731 (   0) : Шегелов-Валерий-Николаевич
*  De-duping author   102733 (   0) : Шегелов-Валерий-Николаевич
*  De-duping author   103171 (   0) : Ливанов-Василий-
*  De-duping author   103173 (   0) : Ливанов-Василий-
*  De-duping author   102777 (   0) : Шкляр-Василь-
*  De-duping author   102779 (   0) : Шкляр-Василь-
*  De-duping author   102781 (   0) : Шкляр-Василь-
*  De-duping author   102991 (   0) : Пекелис-Виктор-
*  De-duping author   102993 (   0) : Пекелис-Виктор-
*  De-duping author   103687 (   0) : Петров-Владимир-Федорович
*  De-duping author   103689 (   0) : Петров-Владимир-Федорович
*  De-duping author   103063 (   0) : Айдинов-Г-
*  De-duping author   103065 (   0) : Айдинов-Г-
*  De-duping author   102995 (   0) : Газета-Газета-Литературная
*  De-duping author   102997 (   0) : Газета-Газета-Литературная
*  De-duping author   102735 (   0) : Борисович-Гайворонский-Александр
*  De-duping author   102737 (   0) : Борисович-Гайворонский-Александр
*  De-duping author   102739 (   0) : Борисович-Гайворонский-Александр
*  De-duping author   102959 (   0) : Лодзинської-Галини-
*  De-duping author   102961 (   0) : Лодзинської-Галини-
*  De-duping author   103155 (   0) : т-д-с
*  De-duping author   103157 (   0) : т-д-с
*  De-duping author   102949 (   0) : Ковалев-Дм-
*  De-duping author   102951 (   0) : Ковалев-Дм-
*  De-duping author   102649 (   0) : Гаврилович-Дяченко-Петро
*  De-duping author   102651 (   0) : Гаврилович-Дяченко-Петро
*  De-duping author   102971 (   0) : Морозова-Елена-Ю
*  De-duping author   102973 (   0) : Морозова-Елена-Ю
*  De-duping author   103191 (   0) : Шаретт-Ж-
*  De-duping author   103193 (   0) : Шаретт-Ж-
*  De-duping author   103055 (   0) : Арно-Жорж-
*  De-duping author   103057 (   0) : Арно-Жорж-
*  De-duping author   102751 (   0) : Е-Жуковская-Е
*  De-duping author   102753 (   0) : Е-Жуковская-Е
*  De-duping author   102755 (   0) : Е-Жуковская-Е
*  De-duping author   102757 (   0) : Е-Жуковская-Е
*  De-duping author   103175 (   0) : Б,а,х,т,и,н-И,-
*  De-duping author   103177 (   0) : Б,а,х,т,и,н-И,-
*  De-duping author   103467 (   0) : Маслюков-Иван-Валентинович
*  De-duping author   103469 (   0) : Маслюков-Иван-Валентинович
*  De-duping author   103003 (   0) : Митрополит-Иерофей (Влахос)-
*  De-duping author   103005 (   0) : Митрополит-Иерофей (Влахос)-
*  De-duping author   103021 (   0) : КУЗНЕЦОВ-ИСАЙ-
*  De-duping author   103023 (   0) : КУЗНЕЦОВ-ИСАЙ-
*  De-duping author   103183 (   0) : Караджале-Йон-Лука
*  De-duping author   103185 (   0) : Караджале-Йон-Лука
*  De-duping author   103015 (   0) : Льис-Клайв-Стейплз
*  De-duping author   103017 (   0) : Льис-Клайв-Стейплз
*  De-duping author   102759 (   0) : Костин-Константин-
*  De-duping author   102761 (   0) : Костин-Константин-
*  De-duping author   102763 (   0) : Костин-Константин-
*  De-duping author   102765 (   0) : Костин-Константин-
*  De-duping author   102767 (   0) : Костин-Константин-
*  De-duping author   103047 (   0) : Огульчанская-Л-
*  De-duping author   103049 (   0) : Огульчанская-Л-
*  De-duping author   103123 (   0) : Дербенев-Леонид-
*  De-duping author   103125 (   0) : Дербенев-Леонид-
*  De-duping author   103039 (   0) : Будогоская-Лидия-
*  De-duping author   103041 (   0) : Будогоская-Лидия-
*  De-duping author   102637 (   0) : Маликова-Мария-
*  De-duping author   102639 (   0) : Маликова-Мария-
*  De-duping author   103111 (   0) : Виноградова-Мария-М
*  De-duping author   103113 (   0) : Виноградова-Мария-М
*  De-duping author   103087 (   0) : Лавлейс-Мерилин-
*  De-duping author   103089 (   0) : Лавлейс-Мерилин-
*  De-duping author   102663 (   0) : Голубець-Микола-
*  De-duping author   102665 (   0) : Голубець-Микола-
*  De-duping author   102667 (   0) : Голубець-Микола-
*  De-duping author   103025 (   0) : Митьки-Митьки-
*  De-duping author   103027 (   0) : Митьки-Митьки-
*  De-duping author   103083 (   0) : Ромм-Михаил-Д
*  De-duping author   103085 (   0) : Ромм-Михаил-Д
*  De-duping author   102935 (   0) : Козлова-Наталия-Н
*  De-duping author   102937 (   0) : Козлова-Наталия-Н
*  De-duping author   102999 (   0) : Плавильщиков-Николай-
*  De-duping author   103001 (   0) : Плавильщиков-Николай-
*  De-duping author   102955 (   0) : ФИНКЕЛЬШТЕЙН-Норман-Дж
*  De-duping author   102957 (   0) : ФИНКЕЛЬШТЕЙН-Норман-Дж
*  De-duping author   103091 (   0) : де Лиль-Адан-Огюст-Вилье
*  De-duping author   103093 (   0) : де Лиль-Адан-Огюст-Вилье
*  De-duping author   102657 (   0) : Нестайко-Омелян-
*  De-duping author   102659 (   0) : Нестайко-Омелян-
*  De-duping author   102661 (   0) : Нестайко-Омелян-
   De-duping author   103517 (   0) : Сэльер-Петти-
*  De-duping author   102939 (   0) : сетевой журналист-С-Весельчаков
*  De-duping author   102941 (   0) : сетевой журналист-С-Весельчаков
*  De-duping author   103099 (   0) : Коноплев-Сергей-
*  De-duping author   103101 (   0) : Коноплев-Сергей-
*  De-duping author   103179 (   0) : Владислав-Сосновский-
*  De-duping author   103181 (   0) : Владислав-Сосновский-
*  De-duping author   102769 (   0) : Винокурова-Садиченко-Тетяна-
*  De-duping author   102771 (   0) : Винокурова-Садиченко-Тетяна-
*  De-duping author   102773 (   0) : Винокурова-Садиченко-Тетяна-
*  De-duping author   102775 (   0) : Винокурова-Садиченко-Тетяна-
*  De-duping author   103127 (   0) : Мартин-Томас-
*  De-duping author   103129 (   0) : Мартин-Томас-
*  De-duping author   103031 (   0) : А-Харлампиев-А
*  De-duping author   103033 (   0) : А-Харлампиев-А
*  De-duping author   102741 (   0) : Шелегов-Шелегов-Николаевич
*  De-duping author   102743 (   0) : Шелегов-Шелегов-Николаевич
*  De-duping author   102745 (   0) : Шелегов-Шелегов-Николаевич
*  De-duping author   102747 (   0) : Шелегов-Шелегов-Николаевич
*  De-duping author   102749 (   0) : Шелегов-Шелегов-Николаевич
*  De-duping author   103051 (   0) : Бреттшнейдер-Э-
*  De-duping author   103053 (   0) : Бреттшнейдер-Э-
*  De-duping author   103095 (   0) : Коноплева-Ю-
*  De-duping author   103097 (   0) : Коноплева-Ю-
*  De-duping author   102673 (   0) : Киричук-Юр?й-
*  De-duping author   102675 (   0) : Киричук-Юр?й-
*  De-duping author   102677 (   0) : Киричук-Юр?й-
*  De-duping author   102785 (   0) : Тис-Крохмалюк-Юр?й-
*  De-duping author   102787 (   0) : Тис-Крохмалюк-Юр?й-
*  De-duping author   102789 (   0) : Тис-Крохмалюк-Юр?й-
*  De-duping author   102791 (   0) : Тис-Крохмалюк-Юр?й-
*  De-duping author   103201 (   0) : Линковой-Я-
*  De-duping author   103203 (   0) : Линковой-Я-

Archives processing - 27 file(s) [d:\librusec\local]

Processing - "284000-284999.zip"       - done in 00:00:00 (323:0:0 records)
Processing - "285000-285999.zip"       - done in 00:00:00 (397:2:0 records)
Processing - "286000-286999.zip"       - done in 00:00:01 (355:0:0 records)
Processing - "287000-287999.zip"       - done in 00:00:00 (414:0:0 records)
Processing - "fb2-000024-036768.zip"   - done in 00:00:24 (27673:0:0 records)
Processing - "fb2-036769-068941.zip"   - done in 00:00:20 (23571:0:0 records)
Processing - "fb2-068942-080359.zip"   - done in 00:00:08 (8883:0:0 records)
Processing - "fb2-080360-102538.zip"   - done in 00:00:08 (9033:16:0 records)
Processing - "fb2-102539-113575.zip"   - done in 00:00:07 (7828:6:0 records)
Processing - "fb2-113576-122293.zip"   - done in 00:00:07 (7089:14:0 records)
Processing - "fb2-122294-136229.zip"   - done in 00:00:06 (7225:15:0 records)
Processing - "fb2-136230-146556.zip"   - done in 00:00:07 (6679:26:0 records)
Processing - "fb2-146557-152808.zip"   - done in 00:00:04 (5342:3:0 records)
Processing - "fb2-152809-160856.zip"   - done in 00:00:06 (5984:45:0 records)
Processing - "fb2-160858-166622.zip"   - done in 00:00:04 (4738:25:0 records)
Processing - "fb2-166623-172136.zip"   - done in 00:00:05 (5123:0:0 records)
Processing - "fb2-172137-179229.zip"   - done in 00:00:05 (5340:0:0 records)
Processing - "fb2-179230-186929.zip"   - done in 00:00:05 (5652:0:0 records)
Processing - "fb2-186930-195602.zip"   - done in 00:00:05 (5575:0:0 records)
Processing - "fb2-195603-203685.zip"   - done in 00:00:05 (5616:0:0 records)
Processing - "fb2-203686-210960.zip"   - done in 00:00:04 (5084:0:0 records)
Processing - "fb2-210961-218193.zip"   - done in 00:00:05 (5270:0:0 records)
Processing - "fb2-218197-231006.zip"   - done in 00:00:04 (4567:7:0 records)
Processing - "fb2-231008-248050.zip"   - done in 00:00:05 (5743:0:0 records)
Processing - "fb2-248053-262447.zip"   - done in 00:00:04 (4974:0:0 records)
Processing - "fb2-262448-272488.zip"   - done in 00:00:05 (4657:0:0 records)
Processing - "fb2-272490-283998.zip"   - done in 00:00:03 (4121:0:0 records)

Complete processing took 00:03:10

Если вы хотите создать файл "daily_update.zip", который позволяет добавить
дневные архивы Либрусека к уже имеющейся коллекции без ее полной перестройки,
используйте ключ "update". Выдача команды
“lib2inpx.exe --process fb2 --update 167585-167678 --archives d:\librusec\local d:\librusec\sql”
приведет к созданию daily_update.zip в котором будут содержаться INP для всех имеющихся
в директории дневных архивов от "167585-167678.zip" до более поздних. Архивы с именами,
начинающимися с "fb2-", "usr-", а так же более ранние дневные архивы будут игнорироваться.
Этот режим работает только для FB2 и MyHomeLib версии 1.6 и позже - созданный файл
нужно скопировать в директорию MyHomeLib и инициировать update.

При анализе архивов программа намеренно игнорирует "soft links" unless --follow-links
option is specified (MyHomeLib этого не делает), что позволяет определенную гибкость
при хранении архивов. Например можно расположить архивы, общие для flibusta и librusec
в базовой директории и хранить дневные обновления, отличающиеся для библиотек отдельно
друг от друга, воспользовавшись "soft links" для создания путей с полными библиотеками
для MyHomeLib. При этом при создании INPX для таких коллекций опция --archives позволяет
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

Ключ "--sequence" позволяет решить, какие типы серий из базы Либрусека предпочесть:
авторские, издательские или не обращать на тип внимания вообще. Этот ключ работает 
только с новыми базами Либрусека (--db-format=2011-11-06)

При указании "--prefer-fb2=merge" программа будет читать FB2 из архива всегда и 
пытаться подставить серию из FB2 даже если книга имеется в базе. Этот режим 
работает существенно медленнее.

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

Опции --db-format=2010-02-06 (2010-03-17) позволят обработать базы данных с изменившимся после
5 февраля 2010 года форматом (творчество Librusec).

Опция --clean-authors позволит правильно обработать libavtorname.sql от librusec, который
нарушает uniqueness constrain и содержит повторяющиеся записи авторов. При этом
все книги будут доступны.

Опция --clean-aliases позволит правильно обработать lib.libavtoraliase.sql от flibusta.

Опция --inpx-format=2.x приведет к тому, что в INPX файле появится collection.info,
содержащая ту же информацию, что и комментарий архива (новый MyHomeLib).

Ключ --quick-fix обрежет поля, которые при импорте в MyHomeLib не поместятся в базу.
По умолчанию будет использоваться длина из MyHomeLib 1.6.2. Используя --fix-config
можно специфицировать другие размеры полей. Список поддерживаемых проверок и пример
находится в файле limits.conf.

Оставшиеся ключи программы не особенно важны:

--clean-when-done удалит созданную при работе MYSQL базу данных,

--ignore-dump-date проигнорирует дату в дамп файлах и использует сегодняшнее число (UTC)

--inpx позволит указать имя и путь желаемого INPX файла.

Если нужно запускать программу из batch файла много раз на одном и том же наборе MYSQL дампов можно
воспользоваться ключом --no-import и создавать базу данных один раз.

В ключе --comment можно задать путь к файлу с шаблоном для INPX комментария (UTF-8, единственный "%s" будет
заменен на имя генерируемого INPX файла. Если требуется просто включить символ "%" - его нужно удвоить. Для
примера смотрите файлы с расширением utf8 в дистрибутиве программы). Если этого параметра нет, то комментарии
будут генериться в формате, максимально приближенном к тем, что использует MyHomeLib для "стандартных" коллекций
Либрусека. Содержимое комментария к zip файлу (каковым и является результатирующий INPX) и файла collection.info
внутри INPX (для второй версии MyHomeLib, BOM добавляется в collection.info автоматически) совпадают - комментарий
к zip файлу поддерживается по историческим причинам и не используется последними версиями MyHomeLib.

В комплект теперь входит программа позволяющая аккуратно нарезать библиотечные
ZIP архивы. Ее использование максимально просто, обычно нужно просто указать директорию
с FB2 файлами (например содержащую все книги flibusta) и директорию для результатирующих
архивов.

Tool to prepare library archives
Version 1.5

Usage: libsplit.exe [options]

options:
  --help                Print help message
  --from arg            Directory with fb2 books
  --to arg              Directory to put resulting archives into
  --size arg (=2000)    Individual archive size in MB, if greater than 2GB -
                        Zip64 archive will be created
  --text                Open books in text mode

Архивы будут содержать упорядоченные по номерам книги и иметь правильные имена.

Имеется и очень простая утилита для загрузки новых библиотечных архивов и баз данных (синхронизация)

Tool to download library updates
Version 2.0

Usage: D:\Books_e\lib2inpx\libget2.exe [options]

  -config="D:\\Books_e\\lib2inpx\\libget2.conf": configuration file
  -continue=false: continue getting a partially-downloaded file
  -library="flibusta": name of the library profile
  -nosql=false: do not download database
  -retry=3: number of re-tries
  -timeout=20: communication timeout in seconds
  -to="D:\\Books_e\\lib2inpx": destination directory for archives
  -tosql="": destination directory for database files
  -verbose=false: print detailed information

Она разбирает имена файлов в директории, указанной параметром --to, находит из
них ID последней книги и пытается загрузить из указанной библиотеки архивы, содержащие книги с
большими ID. Все решения принимаются исключительно на основании анализа имен файлов.
После загрузки архивов проверяется их integrity. Так же могут быть загружены в директорию --tosql 
и распакованы SQL таблички библиотеки. Kонфигурация для программы лежит в файле libget.conf (JSON),
где можно теперь указать и proxy (поддерживается SOCK5 с authentication и без, что позволяет
качать файлы через Tor или VPN).

В предлагаемый дистрибутив в качестве примера входят 2 PowerShell скрипта: fb2_librusec.ps1
и fb2_flibusta.ps1, которыe принимают единственный параметр – путь к локальным архивам:
“fb2_flibusta.ps1 d:\library\local”.  При выполнении каждый скрипт

1. Загрузит недостающие дневные обновления (в d:\library\local\librusec или
   d:\library\local\flibusta)
2. Загрузит сегодняшние дампы базы данных и распакует их
3. Запустит lib2inpx с обработкой архивов – полученный INPX будет лежать в поддиректории data

Вы сможете посмотреть логи в файлах *.log в директории, откуда запущен скрипт.

Осталось создать новую коллекцию в MyHomeLib или запустить "myhomelib.exe /clear",
чтобы начать все заново :)
