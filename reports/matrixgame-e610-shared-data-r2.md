# MatrixGame E610 r2: общий Rangers DATA

Контрольная ветка: `codex/matrixgame-e610-freeze-20261102`.
Исходники r2: `8c3612d2c347ffcc26d76bf12632f5d2d58d78b0`.

## Изменение

По подтверждённому пользователем пути RangersHostSession теперь по умолчанию
читает `sdmc:/switch/space-rangers-hd-a-war-apart/game`. DATA, CFG и install-файлы
переиспользуются из уже подготовленного каталога основной игры; ещё одна копия
всех 2+ ГБ в `matrixgame-diag/rangers` не требуется. Позиционные аргументы
MatrixGame root и Rangers root по-прежнему позволяют явно задать оба пути.

MatrixGame root сохранён: `sdmc:/switch/matrixgame-diag/game`, ровно robots.pkg
и два оригинальных текстовых CFG (~233 МБ при отсутствии прежней копии).
Он отделён от исходной установки, чтобы не заменять игровые CFG. Запись журналов,
кеша и временной конфигурации остаётся внутри уникальных каталогов
`sdmc:/switch/matrixgame-diag/logs/<run-id>/`.

Никакие ресурсы или файлы на Switch агент не копировал и не изменял.
Основная NRO, прежние E610 NRO и ZIP не пересобирались и не заменялись.
Новой игровой функциональности, заглушек и изменения логики боя нет.

## Первое аппаратное наблюдение пользователя

Пользователь запустил обе прежние E610 NRO без подготовленных ресурсов.
Наблюдал текст FAIL, перевёрнутый по горизонтали и вертикали; тестовую картинку
не наблюдал. Это сообщение пользователя, а не анализ framebuffer/logs.
Точная фаза и причина FAIL неизвестны до получения run.jsonl; отсутствие
картинки глазами не доказывает, что GPU smoke не исполнялся до ошибки.

Это частичное аппаратное наблюдение о выводе ошибки, не PASS Init, карты,
физического HID, полноценного GPU либо cleanup. r2 меняет только источник
Rangers-ресурсов; исправление ориентации не заявляется. Массового переворота
матриц, UV или viewport по одному этому наблюдению не вводилось.

## Проверка и следующий запуск

Используется свежий каталог `build/matrixgame/e610-shared-data-r2/1` и те же
обычные отдельные SDK-цели Quick/Interactive, без основной NRO. Полные боевые
сценарии и регрессии не повторяются: поведение сессии и графики не изменено.
Точные команды, source hashes, SDK link и структура NRO фиксируются в result.json,
native-diagnostic-build.json и manifest нового пакета.

[Инструкция r2](../docs/matrixgame-e610-shared-data-readme.ru.md) описывает два
ручных запуска, точные пути, управление и сбор журналов. Следующий шаг —
пользовательский запуск с ресурсами и передача run.jsonl, фото FAIL и кадров,
если появятся. Campaign/Gov entry и победа остаются вне этих standalone NRO.

## Итог SDK/package

Compile/link, отсутствие неразрешённых strong symbols, NRO0/ASET/icon/NACP,
встроенные шрифты, соответствие ELF/NRO build ID и хешей входов — PASS.
Пути shared Rangers root, отдельного MatrixGame root и logs найдены в обеих ELF.
683 source hashes проверены; 143 прежних файла dist/logs/saves, включая старый
E610 ZIP и все шесть его файлов, сохранены неизменными.

Build ID: `8c3612d2c347-21120b4e60dc014c05ec`.
Новый пакет: `dist/matrixgame-e610-switch-test-r2-shared-data.zip` (10046459 байт).
SHA256: `1925e0942bfa77a157720424bef3b4d0dedde29b7d88fc909a0d74741f313ad2`.

| NRO | SHA256 | ELF build ID |
| --- | --- | --- |
| Quick | `e662661530354e43be2c6f2b9fc62a468ef1e4098e06253a65cdd773c1d6c6d0` | `35950aa532b8993a44afe213aaf80f5d8e2a721f` |
| Interactive | `9db5b12edf0b47754be23812bf5f52c85b60b099db30ed688dbbed5b9827b630` | `5ff043a2568797afa0e2ab421ea66b7e5f04938f` |

CRC, шесть файлов ZIP и побайтовое соответствие папке пакета проверены.
[Машиночитаемое evidence](matrixgame-e610-shared-data-r2-evidence.json).
Итог: **SDK/PACKAGE PASS / HARDWARE PENDING; FAIL TEXT ORIENTATION OPEN**.
