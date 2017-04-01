/*
UEMS - Universidade Estadual de Mato Grosso do Sul
Curso de Bacharelado em Ciência da Computação
Redes de Computadores
Trabalho T1 (Servidor)

Feito por Gabriel de Biasi, RGM 24785.

*/

#define FTP_BUFFER 450
#define QTD_DE_USERS 5

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>


void sigchld_handler() {
  while (waitpid(-1, NULL, WNOHANG) > 0);
}


/*
FUNÇÃO envia(int socket, void* dados, size_t tam)
Esta função é para evitar a fadiga de testar o retorno da função write toda hora.
*/
int envia(int socket, void* dados, size_t tam) {
  int ret;
  if((ret = write(socket, dados, tam)) < 0) {
      perror("Erro no Write!");
  } 
  return ret;
}

/*
FUNÇÃO recebe(int socket, void* dados, size_t tam)
Mesmo caso da função envia, para testar o retorno da função.
*/
int recebe(int socket, void* dados, size_t tam) {
  int ret;
  if((ret = read(socket, dados, tam)) < 0) {
      perror("Erro no Read!");
  }
  return ret;
} 

/*
ROTINA PRINCIPAL
*/
int main(int argc, char *argv[]) {
  char buffer[500];
  int pid, socket_desc, client_sock, c, i, buff;
  long total, tamanho;
  FILE *dir;
  DIR *test;
  char semibuff[460];
  struct sockaddr_in server, cliente;
  
  if(argc != 2) { /* Validação de entrada com argumentos. */
      printf("\n[FTP Server] Erro! Inicie o programa com o argumento: [porta].\n\n");
      exit(1);
  }

  socket_desc = socket(AF_INET, SOCK_STREAM, 0); /* Criação do socket principal. */
  if (socket_desc == -1) {
      printf("[FTP Server] Erro no Socket.\n");
      exit(1);
  }
   
  /* Atribui as variáveis para a estrutura do servidor. */ 
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(atoi(argv[1])); /* Captura a porta pelo argumento de entrada. */
  
  /* Faz a ligação da porta com o socket. (Bind) */
  if(bind(socket_desc, (struct sockaddr*) &server, sizeof(server)) < 0) {
      printf("[FTP Server] Erro no Bind.\n");
      exit(1);
  }
   
  /* Coloca o servidor para ouvir o canal de comunicação. */
  listen(socket_desc, QTD_DE_USERS);
   
  c = sizeof(struct sockaddr_in);
  signal(SIGCHLD, sigchld_handler); /* Ativa o manipulador de sinais. */
  
  printf("\n[FTP Server] Esperando por chamadas de conexao...\n\n");  
  while(1) {      
      client_sock = accept(socket_desc, (struct sockaddr *)&cliente, (socklen_t*)&c); 
      if (client_sock < 0) {
          perror("[FTP Server] Erro na Conexao.\n");
          exit(1);
      }   
      pid = fork();
      if(pid > 0) { /* Criação do processo filho. Nesta condição, é o código do processo pai.*/
          close(client_sock); /* Fecha o socket do cliente no processo pai, pois haverá um filho que irá tratar este cliente. */
          printf("[FTP Server] Cliente [%s:%d] Conectado!\n[FTP Server] Processo [%d] iniciado.\n\n", inet_ntoa(cliente.sin_addr), ntohs(cliente.sin_port), pid); 
      } else if(pid == 0) { /* A partir deste ponto, começa o processo filho ouvindo o cliente específico. */
          close(socket_desc); /* Fecha o socket de servidor, pois a conexão já está feita no "client_sock". */
          while((c = read(client_sock, &i, sizeof(i))) > 0) {
              bzero(buffer, 500); /* Zera o buffer a cada comando e lê a opção escolhida pelo cliente. Caso não receba, encerra o filho. */
              switch(i) {
                  case 1: /* Comando get no servidor. */
                      recebe(client_sock, &buffer, sizeof(buffer)); /* Recebe o nome do arquivo desejado pelo cliente. */
                      if((test = opendir(buffer)) != NULL) { /* Verifica se não é um diretório. */
                          closedir(test);
                          total = -1;
                          envia(client_sock, &total, sizeof(long));
                      } else if((dir = fopen(buffer, "rb") ) != NULL) { /* Verifica se o arquivo existe e se é possível abrí-lo. */
                          /* Se existe, é calculado o tamanho total do arquivo e enviado ao cliente. */
                          fseek(dir, 0, SEEK_END);
                          total = ftell(dir);
                          envia(client_sock, &total, sizeof(long)); 
                          rewind(dir);
                          /* Laço de envio do arquivo. Com um determinado tamanho
                          de buffer, o laço envia os pedaços do arquivo e salva a quantidade de bytes
                          enviados na variável "tamanho", até que ela atinja o tamanho do arquivo. */
                          tamanho = 0;
                          do { 
                              buff = ((total-tamanho) < FTP_BUFFER) ? (total-tamanho) : FTP_BUFFER;
                              fread(buffer, buff, 1, dir); /* Lê um pedaço do arquivo e envia. */
                              buff = envia(client_sock, &buffer, buff);
                              tamanho += buff; /* Guarda a quantidade de bytes enviados de cada write. */
                          } while(total > tamanho);
                          fclose(dir);
                      } else {
                          total = -1;
                          envia(client_sock, &total, sizeof(long));
                      }
                  break;
                  case 2: /* Comando put no servidor. */
                      recebe(client_sock, &buffer, sizeof(buffer)); /* Recebe o nome do arquivo que o cliente irá enviar. */
                      if((dir = fopen(buffer, "wb")) != NULL) {
                          envia(client_sock, &i, sizeof(i)); /* Envia uma flag informando que poderá enviar o arquivo. */
                          recebe(client_sock, &total, sizeof(long)); /* Recebe o tamanho total do arquivo. */
                          /* Inicio do laço de recebimento. A cada iteração um buffer 
                          é lido do socket e é escrito no arquivo. A iteração termina
                          quando a quantidade de bytes recebidos chegar ao tamanho total do arquivo. */
                          tamanho = 0;    
                          do {
                              buff = recebe(client_sock, &buffer, sizeof(buffer));
                              fwrite(buffer, buff, 1, dir);
                              tamanho += buff;
                          } while(total > tamanho);           
                          pclose(dir); /* Fecha o arquivo. */
                      } else {
                          i = -1;
                          envia(client_sock, &i, sizeof(i)); /* Envia esta flag caso o servidor não consiga criar o arquivo. */
                      }                       
                  break;
                  case 3: /* Comando ls no servidor. */
                      recebe(client_sock, &buffer, sizeof(buffer));
                      dir = popen(buffer, "r");
                      bzero(buffer, 500);
                      while((fgets(semibuff, FTP_BUFFER, dir)) != NULL) {
                          strcat(buffer, semibuff);
                      }
                      pclose(dir);
                      envia(client_sock, &buffer, sizeof(buffer));
                  break;
                  case 4: /* Comando cd no servidor. */
                      recebe(client_sock, &buffer, sizeof(buffer));
                      if(chdir(buffer)) { /* Verifica se o diretório existe. */
                          sprintf(buffer, "Erro! Pasta nao encontrada no servidor.");
                      } else {
                          getcwd(buffer, 300);
                      }
                      envia(client_sock, &buffer, sizeof(buffer));
                  break;
                  case 5: /* Comando pwd no servidor. */
                      getcwd(buffer, 300);
                      envia(client_sock, &buffer, sizeof(buffer));
                  break;
              }
          }
          if(c == 0) { /* Caso o cliente desconecte ou cause um erro de conexão. */
              printf("[FTP Server] Cliente [%s:%d] Desconectado!\n[FTP Server] Processo [%d] encerrado.\n\n", inet_ntoa(cliente.sin_addr), ntohs(cliente.sin_port), getpid());
          } else {
              printf("[FTP Server] Cliente [%s:%d] Erro de Conexao!\n[FTP Server] Processo [%d] encerrado.\n\n", inet_ntoa(cliente.sin_addr), ntohs(cliente.sin_port), getpid());
          }
          exit(0);
      } else { /* Caso a criação do fork() não tenha dado certo: Fecha o socket e volta para a espera. */
          close(client_sock);
          continue;
      }
  }
  close(socket_desc);
  return 0;
}
/* Fim :D */
