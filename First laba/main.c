#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "arch.h"

// Вывести в консоль возможные команды
void printPossibleCommands(){
    printf("Возможные аргументы:\n");
        printf("        --pack   <путь к исходной папке> [<путь к выходной папке>]\n");
        printf("        --unpack <путь к исходной папке> [<путь к выходной папке>]\n");
}

int main(int argc, char** argv) {
    // Проверить количество аргументов
    if (argc < 3 && argc > 4) {
        printf("[Ошибка] Указано неверное количество аргументов!\n");
        printPossibleCommands();
	    return 1;
    }

    // Получаем переданные аргументы
    // char* command_name = argv[0]
    char* action = argv[1];
    char* source = argv[2];
    char* output = "";

    // Проверить на существование источник переданного пути
    if (access(source, F_OK) == -1) {
        printf("[Ошибка] %s не существует.\n", source);
        return 1;
    }

    // Если выходной путь не указан
    if (argc == 3) {
        // Если дейтсвие упаковки
        if (strcmp(action, "--pack") == 0) {
            output = "archive.arch";
        }
    } 

    // Если выходной путь указан
    if (argc == 4) {
        output = argv[3];
    }

    // Выполнить определенное действие
    if (strcmp(action, "--pack") == 0) {
        pack(source, output);
    } else {
        if(strcmp(action, "--unpack") == 0) {
            unpack(source, output);
        } else {
            printf("Команды \"%s\" не существует", action);
            printPossibleCommands();
        }
    }

    return 0;
}