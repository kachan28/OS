#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termio.h>
#include <unistd.h>

#include "arch.h"

void pack(char* dir_path, char* archive_path) {
    int archive;           // Архив 
    u64 rootDirNameLength; // Длина названия корневой папки

    // Существует ли в текущей директории архив
    if (access(archive_path, F_OK) != -1) {
        printf("[Предупреждение] %s уже существует.\n", archive_path);
        printf("          Вы хотите переписать его %s?\n", archive_path);
        printf("          Выберите, чтобы продолжить: 'Y' or 'n': ");

        // Ожидание ответа
        int input;
        while((input = _getch())) {
            if (input == 'Y' || input == 'n') {
                break;
            }
        }

        if (input == 'Y') {
            // Удалить существующий архив
            if (unlink(archive_path) == -1) {
                printf("\n\n[Ошибка] Не удалось удалить %s.\n", archive_path);
                exit(1);
            }
        } else {
            // Завершить работу
            exit(1);
        }
    }

    // Создать выходной путь
    archive = open(archive_path, O_CREAT | O_WRONLY, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
    if (archive == -1) {
        printf("[Ошибка] Не удалось создать %s.\n", archive_path);
        exit(1);
    }

    // Добавить в архив идентификатор
    if (write(archive, PREFIX, PREFIX_LENGTH) != PREFIX_LENGTH) {
        printf("[Ошибка] Не удалось добавить идентификатор архива %s.\n", archive_path);
        exit(1);
    }

    // Добавить в архив название корневой папки
    rootDirNameLength = strlen(dir_path);
    if (write(archive, &rootDirNameLength, u64sz) != u64sz) {
        printf("[Ошибка] Не удалось добавить название директории в архив %s.\n", archive_path);
        exit(1);
    }

    // Архивация
    _pack(archive, dir_path);

    // Закрыть ввод в архив
    if (close(archive) == -1) {
        printf("[Ошибка] Не удалось закрыть архив.\n");
        exit(1);
    }
}

void _pack(int archive, char* src_path) {
    // Открыть директорию пути
    DIR *dir = opendir(src_path);

    // Если передан путь к файлу
    if (dir == NULL) {

        // Запись информации о файле
        _pack_info(archive, FILE_NAME, src_path);

        // Запись содержимого файла
        _pack_content(archive, src_path);

        return;
    }

    // Запись информации о корневой директории в архив
    _pack_info(archive, FOLDER_NAME, src_path);

    // Для перехода к нижним директориям
    struct dirent *entry;

    // Пока существует нижняя директория
    while ((entry = readdir(dir)) != NULL) {
        // Пропустить символы '.' или '..'
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Получить текущий путь
        char file_path[MAX_PATH_LENGTH] = {0};
        snprintf(file_path, sizeof(file_path), "%s/%s", src_path, entry->d_name);
        _remove_extra_slash(file_path);

        // Если директория
        if (entry->d_type == DT_DIR) {
            _pack(archive, file_path);
        }
        else {
            // Запись информации о файле
            _pack_info(archive, FILE_NAME, file_path);

            // Запись содержимого файла
            _pack_content(archive, file_path);

            printf("[Упаковано] %s\n", file_path);
        }
    }

    // Закрыть ввод в архив
    if (closedir(dir) == -1) {
        printf("[Ошибка] Не удалось закрыть архив.\n");
        exit(1);
    }
}

void _pack_info(int fd, path_type p_type, char* path) {
    // Взять значение enum
    u8 type = (u8)p_type;

    // Флаг ошибки
    int error = 1;

    do {
        // Запись в архив типа текущего расположения (ФАЙЛ/ДИРЕКТОРИЯ)
        if (write(fd, &type, u8sz) != u8sz) {
            printf("[Ошибка] Невозможно записать тип текущего расположения.\n");
            break;
        }

        // Запись в архив длины пути
        u64 file_path_len = strlen(path) + 1;
        if (write(fd, &file_path_len, u64sz) != u64sz) {
            printf("[Ошибка] Не удалось записать длину пути текущего расположения.\n");
            break;
        }

        // Запись в архив путь
        if (write(fd, path, file_path_len) != file_path_len) {
            printf("[Ошибка] Не удалось записать путь текущего расположения.\n");
            break;
        }

        error = 0;
    } while (0);

    if (error) {
        if (close(fd) == -1) {
            printf("[Ошибка] Не удалось закрыть запись в архив.\n");
        }
        exit(1);
    }
}

void _pack_content(int fd, char* path) {
    int error = 1;     // флаг ошибки
    int file;          // файл
    off_t size;        // размер содержимого файла
    u64 file_size;     // беззнаковый размер содержимого файла
    char content_byte; // байт содержимого файла

    // Открыть файл
    file = open(path, O_RDONLY, S_IRUSR);
    if (file == -1) {
        printf("[Ошибка] Не удалось открыть %s для чтения.\n", path);
        close(fd);
        exit(1);
    }

    do {
        // Получить размер файла
        size = lseek(file, 0, SEEK_END);
        if (size == -1) {
            printf("[Ошибка] Не удалось получить размер %s.\n", path);
            break;
        }
        file_size = (u64)(size);

        // Переместить курсор в начало файла
        if (lseek(file, 0, SEEK_SET) == -1) {
            printf("[Ошибка] Не удалось установить курсор в начало %s.\n", path);
            break;
        }

        // Запись размера содержимого файла в архив
        if (write(fd, &file_size, u64sz) != u64sz) {
            printf("[Ошибка] Не удалось записать размер содержимого %s в архив.\n", path);
            break;
        }

        // Запись содержимого файла в архив
        for (u64 num_bytes = 0; num_bytes < file_size; num_bytes++) {
            // Читаем байт
            if (read(file, &content_byte, 1) != 1) {
                printf("[Ошибка] Не удалось прочитать байт содержимого %s.\n", path);
                break;
            }

            // Записываем байт
            if (write(fd, &content_byte, 1) != 1) {
                printf("[Ошибка] Не удалось записать байт содержимого %s в архив.\n", path);
                break;
            }
        }

        error = 0;
    } while (0);

    // Если запись не удалась
    if (error && close(fd) == -1) {
        printf("[Ошибка] Не удалось закрыть архив.\n");
    }

    // Закрыть файл с содержимым
    if (close(file) == -1) {
        printf("[Ошибка] Не удалось закрыть файл с содержимым %s.\n", path);
        exit(1);
    }

    // Если ошибка, то выйти
    if (error) {
        exit(1);
    }
}

void unpack(char* archive_path, char* out_file) {
    int error = 1;            // Флаг ошибки
    int archive, file;        // Файлы
    char identificator[PREFIX_LENGTH];// Массив символов для идентификатора
    u64 root_dir_len;         // Длина названия корневой директории
    u8 type;                  // Тип пути (ФАЙЛ/ДИРЕКТОРИЯ)
    char* path = NULL;        // Массив для пути файла
    u64 path_len;             // Длина пути файла
    char content_byte;        // Variable for reading file content
    u64 content_len;          // Length of file content in archive
    int read_status;          // Статус чтения из архива
    int write_status;         // Статус записи в архив

    // Открыть файл
    archive = open(archive_path, O_RDONLY);
    if (archive == -1) {
        printf("[Ошибка] Не удалось открыть %s.\n", archive_path);
        exit(1);
    }

    do {
        // Прочитать идентификатор и проверить подлинность
        if (read(archive, &identificator, PREFIX_LENGTH) != PREFIX_LENGTH || strcmp(identificator, PREFIX) != 0) {
            printf("[Ошибка] Исходный файл %s не является архивом.\n", archive_path);
            break;
        }

        // Прочитать длину названия корневой директории
        if (read(archive, &root_dir_len, u64sz) != u64sz) {
            printf("[Ошибка] Не удалось прочитать длину названия корневой директории %s.\n", archive_path);
            break;
        }

        error = 0;
    } while (0);

    // Закрыть поток и выйти
    if (error) {
        close(archive);
        exit(0);
    }

    // Читаем файлы из архива
    while (read(archive, &type, u8sz) == u8sz) {
        error = 1;

        // Прочитать длину пути из архива
        if (read(archive, &path_len, u64sz) != u64sz) {
            printf("[Ошибка] Не удалось прочитать длину пути из %s.\n", archive_path);
            break;
        }

        // Выделить память
        path = (char*)malloc(path_len);
        if (path == NULL) {
            printf("[Ошибка] Не удалось выделить память для пути файла %lu.\n", path_len);
            break;
        }

        // Прочитать файл из архива
        if (read(archive, path, path_len) != path_len) {
            printf("[Ошибка] Не удалось прочитать файл из %s.\n", archive_path);
            free(path);
            break;
        }

        path = _rename_root(path, root_dir_len, out_file);
        if (path == NULL) {
            printf("[Ошибка] Не удалось распаковать архив.\n");
            break;
        }

        // Путь уже существует?
        if (access(path, F_OK) != -1) {
            printf("[Ошибка] %s уже существует.\n", path);
            free(path);
            break;
        }

        // Если ФАЙЛ
        if ((path_type)type == FILE_NAME) {
            // Прочитать длину файлв
            if (read(archive, &content_len, u64sz) != u64sz) {
                printf("[Ошибка] Не удалось прочитать длину файла %s.\n", archive_path);
                free(path);
                break;
            }

            // Открыть файл
            file = open(path, O_CREAT | O_WRONLY, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
            if (file == -1) {
                printf("[Ошибка] Не удалось создать %s.\n", path);
                free(path);
                break;
            }

            // Записать из архива в файл
            for (u64 num_bytes = 0; num_bytes < content_len; num_bytes++) {
                read_status = read(archive, &content_byte, 1);
                write_status = write(file, &content_byte, 1);

                // Обработка ошибой
                if (read_status != 1 || write_status != 1) {
                    printf("[Ошибка] Не удалось записать из %s.\n", archive_path);

                    // Закрыть поток записи
                    if (close(file) == -1) {
                        printf("[Ошибка] Не удалось закрыть поток %s.\n", path);
                    }

                    // Закрыть архив чтения
                    if (close(archive) == -1) {
                        printf("[Ошибка] Не удалось закрыть поток %s.\n", archive_path);
                    }

                    // Освободить память
                    free(path);

                    exit(1);
                }
            }

            printf("[Распаковано] %s\n", path);

            // Закрыть поток
            if (close(file) == -1) {
                printf("[Ошибка] Не удалось закрыть %s.\n", path);
                free(path);
                break;
            }
        }

        if ((path_type)type == FOLDER_NAME) {
            // Создать новую директорию
            if (mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
                printf("[Ошибка] Не удалось создать %s.\n", path);
                free(path);
                break;
            }
        }

        // Осовбодить память
        free(path);

        error = 0;
    }

    // Закрыть архив
    if (close(archive) == -1) {
        printf("[Ошибка] Не удалось закрыть %s.\n", archive_path);
        exit(0);
    }

    if (error) {
        exit(1);
    }

    printf("\n");
}

char* _rename_root(char* path, u64 old_root_len, char* new_root) {
    // Если путь выходной директории пуст - вернуть путь архива 
    if (strcmp(new_root, "") == 0) {
        return path;
    }

    u64 trimmed_path_len = strlen(path) - old_root_len;
    u64 new_root_len = strlen(new_root);

    // Выделить память
    char* renamed = (char*)malloc(new_root_len + trimmed_path_len + 2);

    if (renamed == NULL) {
        free(path);
        return NULL;
    }

    // Добавить '/' в начало пути
    memcpy(renamed, new_root, new_root_len);
    renamed[new_root_len++] = '/';

    memcpy(renamed + new_root_len, path + old_root_len, trimmed_path_len);
    renamed[new_root_len + trimmed_path_len] = '\0';

    _remove_extra_slash(renamed);

    free(path);

    return renamed;
}

int _getch() {
    int character;
    struct termios old_attr, new_attr;

    // Backup terminal attributes
    if (tcgetattr(STDIN_FILENO, &old_attr ) == -1) {
        printf("[ERROR] Couldn't get terminal attributes\n");
        exit(1);
    }

    // Disable echo
    new_attr = old_attr;
    new_attr.c_lflag &= ~(ICANON | ECHO );
    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_attr) == -1) {
        printf("[ERROR] Couldn't set terminal attributes\n");
        exit(1);
    }

    // Get input character
    character = getchar();
    if (character == EOF) {
        printf("[ERROR] Couldn't get user input.\n");
        return 1;
    }

    // Restore terminal attributes
    if (tcsetattr(STDIN_FILENO, TCSANOW, &old_attr) == -1) {
        printf("[ERROR] Couldn't reset terminal attributes.\n");
        exit(1);
    }

    return character;
}

//Удалить лишние '/' из расположения файла (должен быть один)
void _remove_extra_slash(char* path) {
    u64 i = 0;
    u64 renamed_len = strlen(path);

    while (i < renamed_len) {
        if((path[i] == '/') && (path[i + 1] == '/')) {
            for(u64 k = i + 1; k < renamed_len; k++) {
                path[k] = path[k + 1];
            }

            renamed_len -= 1;
        }
        else {
            i++;
        }
    }
}
