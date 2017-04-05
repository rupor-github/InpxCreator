Command line генератор списков для MyHomeLib версии 1.5 и позже и вспомогательные утилиты.

Все программы, запущенные без ключей выводят справку о своих возможностях.

----------------------------------------------------------------------------------------------

Примеры вызовов всех програм можно посмотреть в прилогающихся PowerShell (fb2_flibusta.ps1) 
или bash (fb2_flibusta.sh) скриптах. Все вместе утилиты позволяют накапливать кноги и создавать
INPX индексы автоматически, без ручного вмешательста.

----------------------------------------------------------------------------------------------

libget2 "загружает" index.html с flibusta (librusec), распарсывает его, анализирует уже имеющиеся архивы и 
грузит последние дампы базы данных и новые, не имеющиеся локально дневные обновления библиотек как напрямую, 
так и через socks proxy. Детальная конфигурация в файле libget2.conf. Пример работы:

Downloading flibusta ...

Processing library "flibusta" from configuration "is_flibusta"
Last book id: 480768

Downloading index for http://flibusta.is/daily/      (1) 
Downloading file f.fb2.480769-480806.zip             (1  :000000000000) ++
Downloading file f.fb2.480818-480832.zip             (1  :000000000000) 
Downloading file f.fb2.480833-480845.zip             (1  :000000000000) 
Downloading file f.fb2.480859-480919.zip             (1  :000000000000) +
Processed 4 new archive(s)

Downloading SQL tables for http://flibusta.is/sql/        (1) 
Downloading file lib.libavtor.sql.gz                 (1  :000000000000) 
Downloading file lib.libtranslator.sql.gz            (1  :000000000000) 
Downloading file lib.libavtorname.sql.gz             (1  :000000000000) 
Downloading file lib.libbook.sql.gz                  (1  :000000000000) +++
Downloading file lib.libfilename.sql.gz              (1  :000000000000) 
Downloading file lib.libgenre.sql.gz                 (1  :000000000000) 
Downloading file lib.libgenrelist.sql.gz             (1  :000000000000) 
Downloading file lib.libjoinedbooks.sql.gz           (1  :000000000000) 
Downloading file lib.librate.sql.gz                  (1  :000000000000) 
Downloading file lib.librecs.sql.gz                  (1  :000000000000) 
Downloading file lib.libseqname.sql.gz               (1  :000000000000) 
Downloading file lib.libseq.sql.gz                   (1  :000000000000) 
Processed 12 SQL table(s)

Done...

----------------------------------------------------------------------------------------------

libmerge "накапливает" архив со специальным расширением .merge вливая в него получаемые обновления пока он не достигнет 
заданного размера, после чего архив переименовывается в стандартный .zip (например fb2-474923-477023.zip). Делается это 
быстро, без переупаковки архивов. Пример работы:

Processing archives from "/volume1/backup/library/flibusta;/volume1/backup/library/upd_flibusta"
Last archive: /volume1/backup/library/flibusta/fb2-473308-474922.zip - from 473308 to 474922 size 2000123534
Merge archive: /volume1/backup/library/flibusta/fb2-474923-476977.merging - from 474923 to 476977 size 1960646710
Found updates: 1
	--> /volume1/backup/library/upd_flibusta/f.fb2.476978-477062.zip
	Processing update: /volume1/backup/library/upd_flibusta/f.fb2.476978-477062.zip
	--> Finalizing archive: fb2-474923-477023.zip
	--> New last archive: /volume1/backup/library/flibusta/fb2-474923-477023.zip
	--> Finalizing archive: fb2-477024-477062.merging

Здесь создан новый 2х гигабайтный .zip (fb2-474923-477023.zip) и начат следующий .merge файл (fb2-477024-477062.merging)
в котором будут теперь копиться дневные updates. При сегодняшмих скоростях новый 2-х гигабайтных архив появляется примерно
раз в 25 дней, что позволяет перестраивать INPX индекс примерно раз в месяц.

----------------------------------------------------------------------------------------------

lib2inpx производит INPX индекс из загруженных архивов и дампов базы данных. Пример работы:

Detected MySQL dump date: 2017-03-03

Creating MYSQL database "flibusta_20170303"

Importing - "lib.libavtor.sql"        - done in 00:00:06
Importing - "lib.libtranslator.sql"   - done in 00:00:02
Importing - "lib.libavtoraliase.sql"  - done in 00:00:00
Importing - "lib.libavtorname.sql"    - done in 00:00:08
Importing - "lib.libbook.sql"         - done in 00:01:07
Importing - "lib.libfilename.sql"     - done in 00:00:01
Importing - "lib.libgenre.sql"        - done in 00:00:10
Importing - "lib.libgenrelist.sql"    - done in 00:00:00
Importing - "lib.libjoinedbooks.sql"  - done in 00:00:02
Importing - "lib.librate.sql"         - done in 00:00:24
Importing - "lib.librecs.sql"         - done in 00:00:03
Importing - "lib.libseqname.sql"      - done in 00:00:01
Importing - "lib.libseq.sql"          - done in 00:00:03
Importing - "lib.libsrclang.sql"      - done in 00:00:02

Archives processing - 102 file(s) [/volume1/backup/library/flibusta/]

Processing - "fb2-000024-036768.zip"   - done in 00:00:30 (27671:2:0 records)
Processing - "fb2-036769-068941.zip"   - done in 00:00:25 (23567:4:0 records)
Processing - "fb2-068942-080359.zip"   - done in 00:00:11 (8880:3:0 records)
Processing - "fb2-080360-102538.zip"   - done in 00:00:10 (9030:19:0 records)
Processing - "fb2-102539-113575.zip"   - done in 00:00:10 (7825:9:0 records)
Processing - "fb2-113576-122293.zip"   - done in 00:00:08 (7087:16:0 records)
Processing - "fb2-122294-136229.zip"   - done in 00:00:09 (7224:16:0 records)
Processing - "fb2-136230-146556.zip"   - done in 00:00:08 (6680:25:0 records)
Processing - "fb2-146557-152808.zip"   - done in 00:00:06 (5341:4:0 records)
Processing - "fb2-152809-160856.zip"   - done in 00:00:08 (5981:48:0 records)
Processing - "fb2-160858-166622.zip"   - done in 00:00:06 (4738:25:0 records)
Processing - "fb2-166623-172136.zip"   - done in 00:00:06 (5123:0:0 records)
Processing - "fb2-172137-178932.zip"   - done in 00:00:07 (5932:4:0 records)
Processing - "fb2-178933-185148.zip"   - done in 00:00:07 (5650:0:0 records)
Processing - "fb2-185149-191285.zip"   - done in 00:00:06 (5120:0:0 records)
Processing - "fb2-191286-197830.zip"   - done in 00:00:07 (5182:0:0 records)
Processing - "fb2-197831-203428.zip"   - done in 00:00:05 (4876:0:0 records)
Processing - "fb2-203429-215045.zip"   - done in 00:00:13 (11269:0:0 records)
Processing - "fb2-215046-221195.zip"   - done in 00:00:07 (5288:1:0 records)
Processing - "fb2-221197-225949.zip"   - done in 00:00:05 (4131:0:0 records)
Processing - "fb2-225951-231748.zip"   - done in 00:00:06 (4838:0:0 records)
Processing - "fb2-231750-236931.zip"   - done in 00:00:05 (4336:0:0 records)
Processing - "fb2-236932-242299.zip"   - done in 00:00:06 (4557:0:0 records)
Processing - "fb2-242300-246653.zip"   - done in 00:00:04 (3658:0:0 records)
Processing - "fb2-246654-251199.zip"   - done in 00:00:05 (4058:0:0 records)
Processing - "fb2-251200-256613.zip"   - done in 00:00:06 (4505:0:0 records)
Processing - "fb2-256614-261229.zip"   - done in 00:00:05 (4189:0:0 records)
Processing - "fb2-261230-265929.zip"   - done in 00:00:05 (4156:0:0 records)
Processing - "fb2-265930-270544.zip"   - done in 00:00:05 (4118:0:0 records)
Processing - "fb2-270545-274874.zip"   - done in 00:00:05 (3900:0:0 records)
Processing - "fb2-274875-279689.zip"   - done in 00:00:05 (4360:0:0 records)
Processing - "fb2-279690-284251.zip"   - done in 00:00:05 (4042:0:0 records)
Processing - "fb2-284252-289517.zip"   - done in 00:00:05 (4403:0:0 records)
Processing - "fb2-289518-293964.zip"   - done in 00:00:04 (3852:0:0 records)
Processing - "fb2-293965-298453.zip"   - done in 00:00:05 (3829:0:0 records)
Processing - "fb2-298454-302695.zip"   - done in 00:00:05 (3697:0:0 records)
Processing - "fb2-302696-306017.zip"   - done in 00:00:03 (2790:0:0 records)
Processing - "fb2-306018-309458.zip"   - done in 00:00:04 (3014:0:0 records)
Processing - "fb2-309459-312644.zip"   - done in 00:00:03 (2813:0:0 records)
Processing - "fb2-312645-315894.zip"   - done in 00:00:04 (2804:0:0 records)
Processing - "fb2-315895-319492.zip"   - done in 00:00:04 (3169:0:0 records)
Processing - "fb2-319493-322655.zip"   - done in 00:00:03 (2787:0:0 records)
Processing - "fb2-322656-326782.zip"   - done in 00:00:04 (2993:0:0 records)
Processing - "fb2-326783-329752.zip"   - done in 00:00:03 (2754:0:0 records)
Processing - "fb2-329753-332695.zip"   - done in 00:00:04 (2620:0:0 records)
Processing - "fb2-332696-335512.zip"   - done in 00:00:03 (2457:0:0 records)
Processing - "fb2-335515-338206.zip"   - done in 00:00:02 (2451:0:0 records)
Processing - "fb2-338207-341828.zip"   - done in 00:00:04 (3294:0:0 records)
Processing - "fb2-341829-344700.zip"   - done in 00:00:03 (2741:0:0 records)
Processing - "fb2-344701-347633.zip"   - done in 00:00:04 (2656:0:0 records)
Processing - "fb2-347634-350898.zip"   - done in 00:00:03 (3027:0:0 records)
Processing - "fb2-350899-353393.zip"   - done in 00:00:03 (2351:1:0 records)
Processing - "fb2-353394-355921.zip"   - done in 00:00:03 (2413:0:0 records)
Processing - "fb2-355922-358536.zip"   - done in 00:00:03 (2465:0:0 records)
Processing - "fb2-358537-361208.zip"   - done in 00:00:03 (2456:0:0 records)
Processing - "fb2-361209-363810.zip"   - done in 00:00:02 (2440:0:0 records)
Processing - "fb2-363811-366268.zip"   - done in 00:00:03 (2357:0:0 records)
Processing - "fb2-366269-368725.zip"   - done in 00:00:03 (2292:0:0 records)
Processing - "fb2-368727-370785.zip"   - done in 00:00:02 (1964:0:0 records)
Processing - "fb2-370786-372867.zip"   - done in 00:00:03 (1992:0:0 records)
Processing - "fb2-372868-375645.zip"   - done in 00:00:03 (2616:0:0 records)
Processing - "fb2-375646-378181.zip"   - done in 00:00:03 (2352:0:0 records)
Processing - "fb2-378182-380711.zip"   - done in 00:00:02 (2278:0:0 records)
Processing - "fb2-380712-383308.zip"   - done in 00:00:03 (2285:0:0 records)
Processing - "fb2-383309-386163.zip"   - done in 00:00:03 (2514:0:0 records)
Processing - "fb2-386166-388671.zip"   - done in 00:00:03 (2243:0:0 records)
Processing - "fb2-388672-391102.zip"   - done in 00:00:02 (2142:0:0 records)
Processing - "fb2-391103-393904.zip"   - done in 00:00:03 (2347:0:0 records)
Processing - "fb2-393905-396496.zip"   - done in 00:00:03 (2241:0:0 records)
Processing - "fb2-396497-399298.zip"   - done in 00:00:03 (2533:0:0 records)
Processing - "fb2-399300-401773.zip"   - done in 00:00:03 (2248:0:0 records)
Processing - "fb2-401774-403976.zip"   - done in 00:00:02 (1945:0:0 records)
Processing - "fb2-403977-406089.zip"   - done in 00:00:02 (1868:0:0 records)
Processing - "fb2-406090-407437.zip"   - done in 00:00:02 (1249:0:0 records)
Processing - "fb2-407438-410204.zip"   - done in 00:00:03 (2578:0:0 records)
Processing - "fb2-410205-413563.zip"   - done in 00:00:03 (3103:0:0 records)
Processing - "fb2-413564-415921.zip"   - done in 00:00:03 (2184:0:0 records)
Processing - "fb2-415922-418313.zip"   - done in 00:00:03 (2238:0:0 records)
Processing - "fb2-418314-420562.zip"   - done in 00:00:02 (1952:0:0 records)
Processing - "fb2-420563-423100.zip"   - done in 00:00:03 (2226:0:0 records)
Processing - "fb2-423101-425394.zip"   - done in 00:00:03 (2116:0:0 records)
Processing - "fb2-425395-427663.zip"   - done in 00:00:02 (2013:0:0 records)
Processing - "fb2-427664-430041.zip"   - done in 00:00:03 (2198:0:0 records)
Processing - "fb2-430042-432408.zip"   - done in 00:00:02 (2044:0:0 records)
Processing - "fb2-432409-435095.zip"   - done in 00:00:03 (2290:1:0 records)
Processing - "fb2-435096-437790.zip"   - done in 00:00:03 (2208:0:0 records)
Processing - "fb2-437791-440565.zip"   - done in 00:00:03 (2353:0:0 records)
Processing - "fb2-440566-443196.zip"   - done in 00:00:02 (2228:0:0 records)
Processing - "fb2-443197-445717.zip"   - done in 00:00:03 (1971:0:0 records)
Processing - "fb2-445718-448632.zip"   - done in 00:00:03 (2395:0:0 records)
Processing - "fb2-448633-451708.zip"   - done in 00:00:03 (2635:0:0 records)
Processing - "fb2-451709-453979.zip"   - done in 00:00:02 (2015:0:0 records)
Processing - "fb2-453980-456283.zip"   - done in 00:00:03 (2086:0:0 records)
Processing - "fb2-456284-458487.zip"   - done in 00:00:02 (1996:0:0 records)
Processing - "fb2-458488-460720.zip"   - done in 00:00:02 (2059:0:0 records)
Processing - "fb2-460721-463385.zip"   - done in 00:00:03 (2235:0:0 records)
Processing - "fb2-463386-465915.zip"   - done in 00:00:03 (2273:0:0 records)
Processing - "fb2-465916-468694.zip"   - done in 00:00:02 (2331:0:0 records)
Processing - "fb2-468695-471307.zip"   - done in 00:00:03 (2312:0:0 records)
Processing - "fb2-471308-473307.zip"   - done in 00:00:02 (1673:0:0 records)
Processing - "fb2-473308-474922.zip"   - done in 00:00:02 (1447:0:0 records)
Processing - "fb2-474923-477023.zip"   - done in 00:00:03 (1893:0:0 records)

Complete processing took 00:09:54
