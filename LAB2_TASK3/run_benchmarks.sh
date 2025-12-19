#!/bin/bash
#
# run_benchmarks.sh - Скрипт для автоматических замеров производительности
#
# Сравнивает производительность собственной реализации rwlock
# с библиотечной pthread_rwlock_t на разном количестве потоков.

# Параметры тестирования
INITIAL_KEYS=1000
TOTAL_OPS=100000
SEARCH_PERCENT=0.80
INSERT_PERCENT=0.10
NUM_RUNS=5
THREAD_COUNTS="1 2 4 8"

echo "=============================================="
echo "   BENCHMARK: Custom vs Pthread rwlock"
echo "=============================================="
echo ""
echo "Parameters:"
echo "  Initial keys:    $INITIAL_KEYS"
echo "  Total operations: $TOTAL_OPS"
echo "  Search percent:   $SEARCH_PERCENT"
echo "  Insert percent:   $INSERT_PERCENT"
echo "  Runs per config:  $NUM_RUNS"
echo "  Thread counts:    $THREAD_COUNTS"
echo ""

# Файлы результатов
echo "threads,run,time_sec" > results_custom.csv
echo "threads,run,time_sec" > results_pthread.csv

# Временные файлы для сбора времени
> /tmp/custom_times.txt
> /tmp/pthread_times.txt

echo "Running benchmarks..."
echo ""

for threads in $THREAD_COUNTS; do
    echo "Testing with $threads thread(s)..."
    
    # Очищаем временные файлы для текущего числа потоков
    > /tmp/custom_current.txt
    > /tmp/pthread_current.txt
    
    for run in $(seq 1 $NUM_RUNS); do
        # Custom rwlock - используем sed для правильного парсинга
        time_custom=$(printf "%d\n%d\n%s\n%s\n" $INITIAL_KEYS $TOTAL_OPS $SEARCH_PERCENT $INSERT_PERCENT | ./test_custom $threads 2>/dev/null | grep "Elapsed time" | sed 's/.*= \(.*\) seconds/\1/')
        
        if [ -n "$time_custom" ]; then
            echo "$threads,$run,$time_custom" >> results_custom.csv
            echo "$time_custom" >> /tmp/custom_current.txt
        fi
        
        # Pthread rwlock
        time_pthread=$(printf "%d\n%d\n%s\n%s\n" $INITIAL_KEYS $TOTAL_OPS $SEARCH_PERCENT $INSERT_PERCENT | ./test_pthread $threads 2>/dev/null | grep "Elapsed time" | sed 's/.*= \(.*\) seconds/\1/')
        
        if [ -n "$time_pthread" ]; then
            echo "$threads,$run,$time_pthread" >> results_pthread.csv
            echo "$time_pthread" >> /tmp/pthread_current.txt
        fi
        
        echo "  Run $run: custom=${time_custom}s, pthread=${time_pthread}s"
    done
    
    # Вычисляем среднее с помощью awk (поддерживает e-notation)
    avg_custom=$(awk '{ sum += $1; n++ } END { if (n > 0) printf "%.6e", sum / n; else print "N/A" }' /tmp/custom_current.txt)
    avg_pthread=$(awk '{ sum += $1; n++ } END { if (n > 0) printf "%.6e", sum / n; else print "N/A" }' /tmp/pthread_current.txt)
    
    echo "  Average: custom=${avg_custom}s, pthread=${avg_pthread}s"
    echo "$threads $avg_custom" >> /tmp/custom_times.txt
    echo "$threads $avg_pthread" >> /tmp/pthread_times.txt
    echo ""
done

echo "=============================================="
echo "            SUMMARY RESULTS"
echo "=============================================="
echo ""

# Получаем базовое время (1 поток)
base_custom=$(awk '$1 == 1 {print $2}' /tmp/custom_times.txt)
base_pthread=$(awk '$1 == 1 {print $2}' /tmp/pthread_times.txt)

# Заголовок
printf "%-8s | %-14s | %-14s | %-10s | %-10s | %-10s | %-10s\n" \
    "Threads" "Custom(s)" "Pthread(s)" "Speedup_C" "Speedup_P" "Eff_C(%)" "Eff_P(%)"
echo "---------|----------------|----------------|------------|------------|------------|------------"

# Создаём файл сводки
{
    echo "BENCHMARK RESULTS"
    echo "================="
    echo ""
    echo "Configuration:"
    echo "  Initial keys: $INITIAL_KEYS"
    echo "  Total operations: $TOTAL_OPS"
    echo "  Search: ${SEARCH_PERCENT}, Insert: ${INSERT_PERCENT}, Delete: 0.10"
    echo "  Runs per config: $NUM_RUNS"
    echo ""
    echo "Hardware: macOS Sequoia 15.4.1"
    echo "CPU: 2,3 GHz 8 cores processor Intel Core i9"
    echo ""
    printf "%-8s | %-14s | %-14s | %-10s | %-10s | %-10s | %-10s\n" \
        "Threads" "Custom(s)" "Pthread(s)" "Speedup_C" "Speedup_P" "Eff_C(%)" "Eff_P(%)"
    echo "---------|----------------|----------------|------------|------------|------------|------------"
} > results_summary.txt

# Выводим результаты для каждого числа потоков
while read -r line; do
    threads=$(echo "$line" | awk '{print $1}')
    custom_t=$(echo "$line" | awk '{print $2}')
    pthread_t=$(awk -v t="$threads" '$1 == t {print $2}' /tmp/pthread_times.txt)
    
    # Вычисляем ускорение и эффективность с помощью awk
    speedup_custom=$(awk -v base="$base_custom" -v curr="$custom_t" 'BEGIN { printf "%.2f", base / curr }')
    speedup_pthread=$(awk -v base="$base_pthread" -v curr="$pthread_t" 'BEGIN { printf "%.2f", base / curr }')
    eff_custom=$(awk -v sp="$speedup_custom" -v p="$threads" 'BEGIN { printf "%.1f", (sp / p) * 100 }')
    eff_pthread=$(awk -v sp="$speedup_pthread" -v p="$threads" 'BEGIN { printf "%.1f", (sp / p) * 100 }')
    
    printf "%-8s | %-14s | %-14s | %-10s | %-10s | %-10s | %-10s\n" \
        "$threads" "$custom_t" "$pthread_t" "$speedup_custom" "$speedup_pthread" "$eff_custom" "$eff_pthread"
    
    printf "%-8s | %-14s | %-14s | %-10s | %-10s | %-10s | %-10s\n" \
        "$threads" "$custom_t" "$pthread_t" "$speedup_custom" "$speedup_pthread" "$eff_custom" "$eff_pthread" >> results_summary.txt
done < /tmp/custom_times.txt

echo ""
echo "Results saved to:"
echo "  - results_custom.csv"
echo "  - results_pthread.csv"
echo "  - results_summary.txt"

# Очистка
rm -f /tmp/custom_times.txt /tmp/pthread_times.txt /tmp/custom_current.txt /tmp/pthread_current.txt
