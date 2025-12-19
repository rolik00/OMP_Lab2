# Makefile для лабораторной работы: собственная реализация rwlock
# 
# Цели:
#   all           - собрать обе программы
#   test_custom   - тест с собственным rwlock
#   test_pthread  - тест с библиотечным pthread_rwlock_t
#   clean         - удалить объектные файлы и исполняемые файлы
#   benchmark     - запустить скрипт замеров производительности
#
# Использование:
#   make          - собрать всё
#   make clean    - очистить
#   make benchmark - запустить тесты производительности

# Компилятор и флаги
# Автоопределение ОС
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS
    CC = clang
    CFLAGS = -Wall -Wextra -O2
    LDFLAGS = -lpthread
else
    # Linux
    CC = gcc
    CFLAGS = -Wall -Wextra -O2
    LDFLAGS = -lpthread
endif

# Директории
INCLUDE_DIR = include
SRC_DIR = src

# Пути поиска заголовков
INCLUDES = -I$(INCLUDE_DIR)

# Исходные файлы
COMMON_SRC = $(SRC_DIR)/my_rand.c
CUSTOM_SRC = $(SRC_DIR)/test_custom_rwlock.c $(SRC_DIR)/my_rwlock.c
PTHREAD_SRC = $(SRC_DIR)/test_pthread_rwlock.c

# Исполняемые файлы
CUSTOM_BIN = test_custom
PTHREAD_BIN = test_pthread

# Цель по умолчанию
all: $(CUSTOM_BIN) $(PTHREAD_BIN)

# Сборка теста с собственным rwlock
$(CUSTOM_BIN): $(CUSTOM_SRC) $(COMMON_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

# Сборка теста с библиотечным rwlock
$(PTHREAD_BIN): $(PTHREAD_SRC) $(COMMON_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

# Сборка с отладочным выводом
debug: CFLAGS += -DOUTPUT -DDEBUG -g
debug: clean all

# Очистка
clean:
	rm -f $(CUSTOM_BIN) $(PTHREAD_BIN) *.o

# Запуск бенчмарков
benchmark: all
	chmod +x run_benchmarks.sh
	./run_benchmarks.sh

# Быстрый тест (1 поток, небольшая нагрузка)
test: all
	@echo "=== Testing custom rwlock ==="
	echo "100 10000 0.8 0.1" | ./$(CUSTOM_BIN) 1
	@echo ""
	@echo "=== Testing pthread rwlock ==="
	echo "100 10000 0.8 0.1" | ./$(PTHREAD_BIN) 1

.PHONY: all clean debug benchmark test
