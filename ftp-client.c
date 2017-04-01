/*
UEMS - Universidade Estadual de Mato Grosso do Sul
Curso de Bacharelado em Ciência da Computação
Redes de Computadores
Trabalho T1 (Cliente)

Feito por Gabriel de Biasi, RGM 24785.

*/


#define BARRA_PROGRESSO 0.001
#define FTP_BUFFER 450

#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <dirent.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>

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
PROCEDIMENTO loadbar(int tamanho, long baixado, long total, int reset)
Este procedimento cria uma barra de progresso indicando quanto do arquivo
já foi baixado/enviado.
*/
void loadbar(int tamanho, long baixado, long total, int reset) {
  int x, c;
  double ratio;
  static double repeat = 0; 
  if(reset) { /* Caso para resetar a variável estática "repeat" para reutilizar a função. */
      repeat = 0;
      return;
  }
  ratio = (double) baixado/total;
  
  /* Esta condição é para evitar chamadas demasiadas desta função,
  colocando um mínimo de transferência para atualizar a barra de progresso. */  
  if((ratio-repeat) < BARRA_PROGRESSO) return; 
  
  repeat = ratio;  
  c = ratio * tamanho;  
  printf("%3.1f%% [", ratio*100);

  for (x = 0; x < c; x++) putchar('=');
  for (x = c; x < tamanho; x++) putchar(' ');

  printf("]\n\033[F\033[J"); /* Código para reescrever a linha anterior, dando o efeito de barra de progresso. */
}

/*

Rotina Principal

*/
int main(int argc, char *argv[]) {
  char buffer[500];
  int i, sock, buff;
  long total, tamanho;
  FILE *dir;
  DIR *test;
  struct sockaddr_in server;
  char comando[10], mensagem[100];
  
  /* Verifica se os dados do servidor foram passados por argumento de entrada. */
  if(argc != 3) {
      printf("\n[FTP Client] Erro! Inicie o programa com argumentos: [ip] [porta].\n\n");
      exit(1);
  }
  
  /* Criação do socket. */
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
      printf("\n[FTP Client] Erro no Socket.\n\n");
      exit(1);
  }
  
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(argv[1]); /* Pega o ip do servidor pelo argumento de entrada. */
  server.sin_port = htons(atoi(argv[2])); /* Mesma coisa com a porta. */

  /* Tenta fazer a conexão. */
  if (connect(sock, (struct sockaddr*) &server, sizeof(server)) < 0) {
      printf("\n[FTP Client] Erro na conexao.\n\n");
      exit(1);
  }
  
  printf("\n[FTP Client] Conectado! IP: [%s] Porta: [%s]\n", argv[1], argv[2]);  
  while(1) {
      printf("ftp> ");
      fgets(mensagem, 100, stdin);
      bzero(buffer, 500);
      sscanf(mensagem, "%s %[^\n]s", comando, buffer);
      
      if(!strcmp(comando, "get")) { /* Comando get, para baixar um arquivo do servidor. */
          i = 1;
          envia(sock, &i, sizeof(i));
          envia(sock, &buffer, sizeof(buffer)); /* Envia o nome do arquivo desejado e espera a resposta com o tamanho total dele. */
          recebe(sock, &total, sizeof(long));
          if(total == -1) {
              printf("[get] Arquivo \"%s\" nao existe no servidor.\n", buffer);
          } else {    
              printf("[get] Arquivo \"%s\" tem %ld bytes. Iniciando download...\n", buffer, total);
              if((dir = fopen(buffer, "wb")) != NULL) {
                  /* Inicio do laço de recebimento. A cada iteração um buffer 
                  é lido do socket e é escrito no arquivo. A iteração termina
                  quando a quantidade de bytes recebidos chegar ao tamanho total do arquivo. */
                  tamanho = 0;
                  do {
                      buff = recebe(sock, &buffer, sizeof(buffer));
                      fwrite(buffer, buff, 1, dir);
                      tamanho += buff;
                      loadbar(50, tamanho, total, 0); /* Desenha a barra de progresso. */
                  } while(total > tamanho);           
                  pclose(dir);
                  loadbar(0,0,0,1); /* Reset da função que desenha a barra de progresso. */
                  printf("[get] Download completo.\n");
              } else {
                  printf("[get] Nao foi possivel criar o arquivo.\n");
              }
          }
      } else if(!strcmp(comando, "put")) { /* Comando put, para upar o arquivo para o servidor. */
          i = 2;
          if((test = opendir(buffer)) != NULL) { /* Verifica se não é um diretório. */
              closedir(test);
              printf("[put] Arquivo \"%s\" nao existe!\n", buffer);
          } else if((dir = fopen(buffer, "rb") ) != NULL) { /* Verifica se o arquivo existe e se é possível abrí-lo. */
              envia(sock, &i, sizeof(i));
              envia(sock, &buffer, sizeof(buffer));
              recebe(sock, &i, sizeof(i));
              if(i == 2) {
                  /* É calculado o tamanho total do arquivo e enviado ao servidor. */
                  fseek(dir, 0, SEEK_END);
                  total = ftell(dir);
                  envia(sock, &total, sizeof(long)); 
                  rewind(dir);
                  printf("[put] Arquivo \"%s\" tem %ld bytes. Iniciando upload...\n", buffer, total);
                  /* Laço de envio do arquivo. Com um determinado tamanho
                  de buffer, o laço envia os pedaços do arquivo e salva a quantidade de bytes
                  enviados na variável "tamanho", até que ela atinja o tamanho do arquivo. */
                  tamanho = 0;                
                  do { 
                      buff = ((total-tamanho) < FTP_BUFFER) ? (total-tamanho) : FTP_BUFFER;
                      fread(buffer, buff, 1, dir); /* Lê um pedaço do arquivo e envia. */
                      buff = envia(sock, &buffer, buff);
                      tamanho += buff; /* Guarda a quantidade de bytes enviados de cada "envia". */
                      loadbar(50, tamanho, total, 0); /* Desenha a barra de progresso. */
                  } while(total > tamanho);
                  loadbar(0,0,0,1); /* Reset da função que desenha a barra de progresso. */
                  printf("[put] Upload completo.\n");
              } else {
                  printf("[put] Nao foi possivel criar o arquivo no servidor.\n");
              }       
              pclose(dir);
          } else {
              printf("[put] Arquivo \"%s\" nao existe!\n", buffer);
          }
      } else if(!strcmp(comando, "ls")) { /* Comando ls, listar arquivos da pasta do servidor. */
          i = 3;
          envia(sock, &i, sizeof(i));
          envia(sock, &mensagem, sizeof(mensagem));
          printf("[ls]\n");
          recebe(sock, &buffer, sizeof(buffer));
          printf("%s", buffer);
      } else if(!strcmp(comando, "cd")) { /* Comando !cd, para ir a uma pasta específica do servidor. */
          i = 4;
          if(!strlen(buffer)) {
              printf("[cd] Argumentos invalidos.\n");
              continue;
          }
          envia(sock, &i, sizeof(i));
          envia(sock, &buffer, sizeof(buffer));
          recebe(sock, &buffer, sizeof(buffer));
          printf("[cd] %s\n", buffer);    
      } else if(!strcmp(comando, "pwd")) {
          i = 5;
          envia(sock, &i, sizeof(i));
          recebe(sock, &buffer, sizeof(buffer));
          printf("[pwd] %s\n", buffer);
      } else if(!strcmp(comando, "!ls")) { /* Comando !ls, listar arquivos da pasta do cliente. */
          printf("[!ls]\n");
          dir = popen(mensagem+1, "r"); /* O "+1" é utilizado para 'pular' o ponto de exclamação. */
          while((fgets(buffer, 500, dir)) != NULL) {
              printf("%s", buffer);
          }
          pclose(dir);
      } else if(!strcmp(comando, "!cd")) { /* Comando !cd, para ir a uma pasta específica ou voltar a anterior do cliente. */
          if(chdir(buffer)) { /* Verifica se o diretório existe. */
              printf("[!cd] Erro! Pasta nao encontrada.\n");
          } else {
              getcwd(buffer, 300);
              printf("[!cd] %s\n", buffer);
          }
      } else if(!strcmp(comando, "!pwd")) { /* Comando !pwd, mostrar diretório atual do cliente. */
          getcwd(buffer, 450);
          printf("[!pwd] %s\n", buffer);
      } else if(!strcmp(comando, "quit")) { /* Comando quit, encerrar o programa. */
          printf("[quit] Conexao fechada. Encerrando...\n\n");
          break;
      } else if(!strcmp(comando, "clear")) { /* Comando clear, para limpar a tela. */
          printf("\33[H\33[2J");
      } else {
          printf("[ERRO] Comando ftp invalido.\n");
      }
  }
  /* Caso o cliente escolha o quit, sai do laço infinito, fecha o socket e finaliza o programa. */
  close(sock);
  return 0;
}
/* Fim :D */
