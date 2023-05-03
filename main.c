#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
/*


Comment


*/

int acha_posicao_operador(char *argv);
void executar_background(char *comando, int background);
void executar(char *comando);
void iniciar_operadores(char *comando);

int main(int argc, char *argv[])
{

    if (argc < 2)
    { // verifica se o usuário digitou algum comando
        printf("uso: %s comando\n", argv[0]);
        return 1;
    }

    char *comandos = malloc(1); // aloca o espaço mais 1 byte necessário para o \0
    comandos[0] = '\0';         // inicializa a string com o caractere nulo

    for (int i = 1; i < argc; i++)
    {
        comandos = realloc(comandos, strlen(comandos) + strlen(argv[i]) + 2); // realoca o espaço para o próximo comando
        strcat(comandos, argv[i]);                                            // concatena o comando
        strcat(comandos, " ");                                                // concatena um espaço
    }

    iniciar_operadores(comandos);

    free(comandos);

    return 0;
}

void iniciar_operadores(char *comando)
{
    int posicao_operador = acha_posicao_operador(comando);
    int background = 0;

    if (posicao_operador == -1)
    {
        // Nenhum operador condicional encontrado, execute os comandos em sequência
        executar(comando);
        return;
    }

    // verifica se o operador está no final do comando
    if (posicao_operador == strlen(comando) - 2)
    {
        background = 1;
        executar_background(comando, background);
        return;
    }

    // Loop para achar outros operadores condicionais
    int ignora_anterior = 0, status;
    int posicao_operador_seguinte = -2;
    while (posicao_operador_seguinte != -1) 
    {
        posicao_operador += posicao_operador_seguinte + 2;
        // Separa os comandos correspondentes ao operador condicional
        char comando_anterior[posicao_operador + 1];
        strncpy(comando_anterior, comando, posicao_operador);
        comando_anterior[posicao_operador] = '\0';

        char *comando_seguinte = comando + posicao_operador + 2;

        // Executa o comando anterior
        if (ignora_anterior == 0)
            status = system(comando_anterior);

        // Verifica o resultado do comando anterior e decide se deve executar o próximo comando
        if ((comando[posicao_operador] == '|' && status != 0) || (comando[posicao_operador] == '&' && status == 0))
        {
            iniciar_operadores(comando_seguinte);
        }
        else
        {
            ignora_anterior = 1; // Exemplo de comando --> ping www.unifesp.br "&&" ls "||" ls -la
        }
        posicao_operador_seguinte = acha_posicao_operador(comando_seguinte);
    }
}

int acha_posicao_operador(char *string)
{

    for (int i = 0; i < strlen(string); i++)
    {
        if (string[i] == 124) // 124 = |
        {
            if (string[i + 1] == 124) // verificar se é ||
            {
                return i;
            }
        }
        if (string[i] == 38) // 38 = &
        {
            return i; // Retorna a posição do operador condicional ou de background
        }
    }

    return -1;
}

void executar_background(char *comando, int background)
{
    pid_t pid;
    pid = fork();

    if (background)
    {
        // Remove o '&' do final do comando que rodará em background
        comando[strlen(comando) - 3] = '\0';
    }

    // Divide o comando em um array de strings
    char *arg;
    char **args = malloc(1 * sizeof(*args));
    int argCount = 0;

    arg = strtok(comando, " ");
    while (arg != NULL)
    {
        args[argCount] = malloc(sizeof(char) * strlen(arg) + 2);    // aloca o espaço mais 1 byte necessário para o \0
        strcpy(args[argCount], arg);                                // copia a string para o espaço alocado
        args = realloc(args, ((argCount + 1) * sizeof(*args)) + 2); // realoca o espaço para o próximo comando
        argCount++;
        arg = strtok(NULL, " ");
    }
    args[argCount] = NULL;

    if (pid == 0) // processo filho
    {
        execvp(args[0], args);
    }
    else if (pid > 0) // Processo pai
    {
        int status;
        if (background)
        {
            // Se estiver sendo executado em segundo plano, não espere o processo filho terminar.
            return;
        }
        else
        {
            // Caso contrário, espere o filho terminar
            waitpid(pid, &status, 0);
            if (WEXITSTATUS(status) != 0) //verificação de sucesso de execução do processo filho
            {
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        }
    }

    free(args);
}

void executar(char *comando)
{

    int cont, append;

    char *input_file = NULL;
    char *output_file = NULL;

    int fd_in = STDIN_FILENO;
    int fd_out = STDOUT_FILENO;

    char **comandos = malloc(1 * sizeof(*comandos));
    int num_comandos = 0;

    char *parte = strtok(comando, "|"); // Verificando pipes

    while (parte != NULL)
    {
        comandos[num_comandos] = malloc(sizeof(char) * strlen(parte) + 2);          // aloca o espaço mais 1 byte necessário para o \0
        strcpy(comandos[num_comandos], parte);                                      // copia a string para o espaço alocado
        comandos = realloc(comandos, ((num_comandos + 1) * sizeof(*comandos)) + 2); // realoca o espaço para o próximo comando
        parte = strtok(NULL, "|");                                                  // pega o próximo comando

        num_comandos++;
    }

    comandos[num_comandos] = NULL; // adiciona o NULL no final do vetor de comandos

    // vetor de pipes
    // [[escrita, leitura], [escrita, leitura], ...]
    int fdpipes[num_comandos - 1][2];

    for (int i = 0; i < num_comandos; i++) // para cada comando
    {
        char *comando_atual = strtok(comandos[i], " ");
        int num_parametros = 0;
        char **parametros = malloc(1 * sizeof(*parametros));

        while (comando_atual != NULL)
        {
            parametros[num_parametros] = malloc(sizeof(char) * strlen(comando_atual) + 2);
            strcpy(parametros[num_parametros], comando_atual);
            parametros = realloc(parametros, ((num_parametros + 1) * sizeof(*parametros)) + 2);
            comando_atual = strtok(NULL, " ");

            num_parametros++;
        }

        // verifica se há redirecionamento de entrada/saída
        for (cont = 0; cont < num_parametros; cont++)
        {
            if (strcmp(parametros[cont], "<") == 0)
            {
                input_file = parametros[cont + 1]; // pega o nome do arquivo
                parametros[cont] = NULL;           // remove o parâmetro "<" do vetor de parâmetros
                cont++;
            }
            else if (strcmp(parametros[cont], ">") == 0)
            {
                output_file = parametros[cont + 1]; // pega o nome do arquivo
                parametros[cont] = NULL;            // remove o parâmetro ">" do vetor de parâmetros
                append = 0;
                cont++;
            }
            else if (strcmp(parametros[cont], ">>") == 0)
            {
                output_file = parametros[cont + 1]; // pega o nome do arquivo
                parametros[cont] = NULL;            // remove o parâmetro ">>" do vetor de parâmetros
                append = 1;
                cont++;
            }
        }

        // redirecionamento para entrada padrão
        if (input_file != NULL)
        {
            fd_in = open(input_file, O_RDONLY); // abre o arquivo para leitura
            if (fd_in == -1)
            {
                perror("open input"); // se não conseguir abrir o arquivo, imprime o erro
                exit(EXIT_FAILURE);
            }

            if (dup2(fd_in, STDIN_FILENO) == -1)
            {
                perror("dup2 input"); // se não conseguir redirecionar a entrada padrão, imprime o erro
                exit(EXIT_FAILURE);
            }
        }

        // redirecionamento para saída padrão
        if (output_file != NULL)
        {
            /*
                se append for 0, então o conteúdo é substituído
                se append for 1, então o conteúdo é adicionado
            */
            if (append == 0)
            {
                fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666); // abre o arquivo para escrita, cria o arquivo se não existir, e apaga o conteúdo se existir
            }
            else
            {
                fd_out = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0666); // abre o arquivo para escrita, cria o arquivo se não existir, e adiciona o conteúdo se existir
            }

            if (fd_out == -1)
            {
                perror("open output"); // se não conseguir abrir o arquivo, imprime o erro
                exit(EXIT_FAILURE);
            }

            if (dup2(fd_out, STDOUT_FILENO) == -1)
            {
                perror("dup2 output"); // se não conseguir redirecionar a saída padrão, imprime o erro
                exit(EXIT_FAILURE);
            }
        }

        parametros[num_parametros] = NULL; // adiciona o NULL no final do vetor de parâmetros

        if (i != num_comandos - 1)
        {
            pipe(fdpipes[i]); // cria um pipe antes do próximo comando, exceto para o último comando
        }

        pid_t pid = fork(); // cria um processo filho

        if (pid == 0)
        { // processo filho

            if (i != 0)
            {                                          // se não for o primeiro comando
                dup2(fdpipes[i - 1][0], STDIN_FILENO); // redireciona a entrada padrão para a extremidade de leitura do pipe
                close(fdpipes[i - 1][0]);              // fecha a extremidade de leitura do pipe
            }

            if (i != num_comandos - 1)
            {                                       // se não for o último comando
                dup2(fdpipes[i][1], STDOUT_FILENO); // redireciona a saída padrão para a extremidade de escrita do pipe
            }

            execvp(parametros[0], parametros); // executa o comando atual com os parâmetros correspondentes

            // se execvp retornou, houve um erro
            perror("execvp");
            exit(EXIT_FAILURE);
        }
        else if (pid > 0)
        { // processo pai
            // Espera pelo término dos processos filhos
            waitpid(pid, NULL, 0);

            // fecha os fds de input e output
            if (input_file != NULL)
            {
                if (dup2(STDIN_FILENO, fd_in) == -1)
                {
                    perror("dup2 stdin");
                    exit(EXIT_FAILURE);
                }
                close(fd_in);
                input_file = NULL;
            }

            if (output_file != NULL)
            {
                if (dup2(STDOUT_FILENO, fd_out) == -1)
                {
                    perror("dup2 stdout");
                    exit(EXIT_FAILURE);
                }
                close(fd_out);
                output_file = NULL;
            }

            close(fdpipes[i][1]);
        }
        else
        { // erro ao criar processo filho
            perror("fork");
            exit(EXIT_FAILURE);
        }
        free(parametros);
    }

    free(comandos);
}
