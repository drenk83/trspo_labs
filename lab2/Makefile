# Диспетчер. Сам ничего не компилирует: по очереди вызывает Makefile
# в каталогах библиотек, app/ и elfloader/.
# Библиотеки описаны своими правилами (задание 6.2), без ./configure.
#
# Порядок каталогов фиксированный. libpng и FreeType линкуются
# с уже собранным zlib, поэтому zlib всегда первый.

# .PHONY — цели, которым не соответствует файл. Иначе make мог бы
# решить, что цель «static» уже готова, если в каталоге лежит файл static.
.PHONY: all static dynamic blob clean clean-bins

# Задание: make all удаляет старые бинарники и собирает все три версии.
# elfloader собирается вместе с blob.
all:
	$(MAKE) clean-bins
	$(MAKE) static
	$(MAKE) dynamic
	$(MAKE) blob

# Только три программы. Библиотеки и elfloader не трогаем:
# их пересоберёт обычная проверка времени изменения файлов.
clean-bins:
	rm -f build/app-static build/app-dynamic build/app-blob

# $(MAKE) -C каталог цель  — зайти в каталог и выполнить цель.
# Рецепт идёт сверху вниз, даже при make -j: это один список команд.
static:
	$(MAKE) -C zlib static
	$(MAKE) -C libpng static
	$(MAKE) -C freetype static
	$(MAKE) -C app static

dynamic:
	$(MAKE) -C zlib dynamic
	$(MAKE) -C libpng dynamic
	$(MAKE) -C freetype dynamic
	$(MAKE) -C app dynamic

# blob вшивает те же .a, что и static, и собирает загрузчик.
blob:
	$(MAKE) -C zlib blob
	$(MAKE) -C libpng blob
	$(MAKE) -C freetype blob
	$(MAKE) -C app blob
	$(MAKE) -C elfloader

clean:
	$(MAKE) -C zlib clean
	$(MAKE) -C libpng clean
	$(MAKE) -C freetype clean
	$(MAKE) -C app clean
	$(MAKE) -C elfloader clean
	rm -rf build
