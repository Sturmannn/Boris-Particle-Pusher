#!/bin/bash

# --- ЗАЩИТА ОТ ПАРАЛЛЕЛЬНОГО ЗАПУСКА (FLOCK) ---
# Открываем файловый дескриптор 200 на файл блокировки
# exec 200>/tmp/rsync_cluster.lock

# Пытаемся взять монопольный замок. 
# Флаг -n означает: "если файл занят другим sync.sh, мгновенно завершить работу"
# flock -n 200 || { echo "Синхронизация уже выполняется. Запрос пропущен."; exit 0; }

# --- НАСТРОЙКИ ПОДКЛЮЧЕНИЯ ---
CLUSTER_USER=""  # Впишите сюда ваш логин на кластере
CLUSTER_HOST="10.0.2.20"              # IP-адрес вашего кластера
CLUSTER_DIR="~/Projects/BorisParticlePusher"   # Папка на кластере, куда копировать код
CLUSTER_ALIAS="cluster-x86"

# --- ЗАПУСК СИНХРОНИЗАЦИИ ---
# Объяснение флагов rsync:
# -a (archive): сохраняет права доступа к файлам, владельцев и временные метки.
# -v (verbose): выводит в консоль список файлов, которые были изменены и перенесены.
# -z (compress): сжимает данные в процессе передачи для экономии трафика.
# --delete: удаляет файлы на кластере, если вы удалили их у себя в WSL (держит папки в идеальном соответствии).
rsync -avz --delete \
    --exclude='build/' \
    --exclude='bin/' \
    --exclude='.git/' \
    --exclude='.vscode/' \
    --exclude='rsync_cluster.sh' \
    --exclude='README.md' \
    --exclude='3rdparty' \
    --exclude='.env' \
    --exclude='results' \
    --exclude='make_plots.py' \
    --exclude='plots' \
    ./ ${CLUSTER_ALIAS}:${CLUSTER_DIR}/
    # ./ ${CLUSTER_USER}@${CLUSTER_HOST}:${CLUSTER_DIR}/

echo "=== Синхронизация завершена успешно ==="