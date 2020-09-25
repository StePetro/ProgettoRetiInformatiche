////////////////////////////////////////////////////////////////////////////////    
///                                SERVER                                    ///
////////////////////////////////////////////////////////////////////////////////

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define BUF_SIZE 516
#define BACKLOG_DIM 10
#define PORTA_DEFAULT 69
#define MAX_MODE_SIZE 9
#define DATA_SIZE 512
#define ACK_SIZE 4
#define RRQ 1
#define DATA 3
#define ACK 4
#define ERROR 5
#define FILENONTROVATO 2
#define RICHIESTANONVALIDA 1
#define PORTA 1
#define PERCORSO 2
#define PERCORSODEFAULT "/"

////////////////////////////////////////////////////////////////////////////////    
///                              FUNZIONI                                    ///
////////////////////////////////////////////////////////////////////////////////

//SERIALIZZAZIONE DEL MESSAGGIO DATI ------------------------------------------------------
void serializzaData(int* posizione, uint16_t* opcode, char buffer[], uint16_t *numBlockMsg, int numeroBlocco){
	*posizione=0;
	*opcode = htons(DATA);
	memcpy(buffer,opcode,sizeof(*opcode));
	*posizione += sizeof(*opcode);
	*numBlockMsg = htons(numeroBlocco);
	memcpy(buffer+*posizione,numBlockMsg,sizeof(*numBlockMsg));
	*posizione += sizeof(*numBlockMsg);
	return;
}

//DESERIALIZZAZIONE DEL MESSAGGIO ACK ------------------------------------------------------
void deserializzazioneACK(int* posizione, uint16_t* opcode, char buffer[], uint16_t* numACKmsg, int* numeroACK){
	*posizione=0;
	memcpy(opcode,buffer,sizeof(*opcode));
	*opcode = ntohs(*opcode);
	printf("INF| Il client(processo %d) ha inviato l'opcode: %d\n", getpid(), *opcode); //INFO 
	*posizione += sizeof(*opcode);
	memcpy(numACKmsg,buffer+*posizione,sizeof(*numACKmsg));
	*numeroACK = ntohs(*numACKmsg);
	printf("INF| Il client(processo %d) ha inviato l'ACK per il blocco: %d\n", getpid(), *numeroACK); //INFO 
	return;
}

//RICEZIONE DEL MESSAGGIO ACK ------------------------------------------------------
void ricezioneACK(int* posizione, uint16_t* opcode, int nuovoDescrSocket, char buffer[], uint16_t* numACKmsg, int* numeroACK, int numeroBlocco, int ret, struct sockaddr_in indirizzoClient){
	socklen_t lunghezza = sizeof(indirizzoClient);
	ret = recvfrom(nuovoDescrSocket, (void*)buffer, ACK_SIZE, 0, (struct sockaddr*)&indirizzoClient, &lunghezza);
	if(ret < 0){
    	perror("ERR| Errore in fase di ricezione");
        printf("INF| Fine connessione con il client(processo %d)\n\n", getpid());
        exit(-1);
    }
			
	deserializzazioneACK(posizione, opcode, buffer, numACKmsg, numeroACK);
	if((*opcode != ACK)||*numeroACK!=numeroBlocco){
		printf("ERR| ACK corrotto o non valido\n");
        printf("INF| Fine connessione con il client(processo %d)\n\n", getpid());
        exit(-1);
	}
	return;
}

//INVIO MESSAGGIO DI ERRORE ------------------------------------------------------
void invioMessaggioErrore(int* posizione, uint16_t* opcode, int nuovoDescrSocket, char buffer[], char messaggioErrore[], uint16_t errore, int ret, struct sockaddr_in indirizzoClient){
	*posizione=0; 
	*opcode = htons(ERROR);
	memcpy(buffer,opcode,sizeof(*opcode));
	*posizione += sizeof(*opcode);
	errore = htons(errore);
	memcpy(buffer + *posizione, &errore, sizeof(errore));
	*posizione += sizeof(errore);
	strcpy(buffer + *posizione, messaggioErrore);
	*posizione += strlen(messaggioErrore)+1;

	ret = sendto(nuovoDescrSocket, (void*) buffer, *posizione, 0, (struct sockaddr*)&indirizzoClient, sizeof(indirizzoClient));
	if(ret < 0){
    	perror("ERR| Errore in fase di invio");
		printf("INF| Fine connessione con il client(processo %d)\n\n", getpid());
    }
	return;
}

////////////////////////////////////////////////////////////////////////////////    
///                         STRUTTURE & VARIABILI                            ///
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]){
    int ret, descrSocket, nuovoDescrSocket, posizione, numeroBlocco, numeroACK; 
    uint16_t opcode, numBlockMsg, numACKmsg, porta, nuovaPorta;
    pid_t pid;
	socklen_t lunghezza;
    struct sockaddr_in indirizzoServer, indirizzoClient, nuovoIndirizzo;
    char buffer[BUF_SIZE], mode[MAX_MODE_SIZE], percorsoLocale[BUF_SIZE], nomeFile[BUF_SIZE];
	ssize_t numeroLetti;
	FILE *puntFile;
	if(argc == 3){
		strcpy(percorsoLocale,argv[PERCORSO]);
		printf("\nINF| Inserito il percorso locale: %s\n", percorsoLocale); //INFO
	}else{
		strcpy(percorsoLocale, PERCORSODEFAULT);
		printf("\nINF| Inserito il percorso locale: %s\n", percorsoLocale); //INFO
	}

////////////////////////////////////////////////////////////////////////////////    
///                         SOCKET di ASCOLTO                                ///
////////////////////////////////////////////////////////////////////////////////

    descrSocket = socket(AF_INET, SOCK_DGRAM, 0);
	printf("INF| Socket UDP di ascolto creato: %d\n", descrSocket); //INFO
    
    memset(&indirizzoServer, 0, sizeof(indirizzoServer));
    if(argc>1)
		porta = atoi(argv[PORTA]);
	else
		porta = PORTA_DEFAULT;
	printf("INF| Porta: %d\n", porta); //INFO
	indirizzoServer.sin_family = AF_INET;
	indirizzoServer.sin_port = htons(porta);
    indirizzoServer.sin_addr.s_addr = INADDR_ANY;
	printf("INF| Struttura indirizzo server inizializzata \n"); //INFO
    
    ret = bind(descrSocket, (struct sockaddr*)&indirizzoServer, sizeof(indirizzoServer) );
	if(ret < 0){
        perror("ERR| Errore in fase di bind");
        exit(-1);
    }
	printf("INF| Bind effettuato \n"); //INFO
	printf("INF| Server in ascolto sul socket %d\n", descrSocket); //INFO
    
////////////////////////////////////////////////////////////////////////////////    
///                         GESTIONE CONNESSIONI                             ///
////////////////////////////////////////////////////////////////////////////////

    while(1){
          
		//RICEZIONE RRQ ------------------------------------------------------
		lunghezza = sizeof(indirizzoServer);
	    ret = recvfrom(descrSocket, (void*)buffer, BUF_SIZE, 0, (struct sockaddr*)&indirizzoClient, &lunghezza);
		if(ret < 0){
            perror("ERR| Errore in fase di ricezione");
            exit(-1);
        }
		
		nuovaPorta = 0; //Assegna la porta in automatico se il valore è 0
		printf("\nINF| Nuova richiesta di connessione\n"); //INFO
        
		pid = fork();
        
        //FIGLIO ------------------------------------------------------
        if( pid == 0 ){
            printf("INF| Gestita dal processo %d\n", getpid()); //INFO

			//SOCKET DI GESTIONE
			nuovoDescrSocket = socket(AF_INET, SOCK_DGRAM, 0);
			printf("\nINF| Socket UDP di gestione creato: %d\n", descrSocket); //INFO
			memset(&nuovoIndirizzo, 0, sizeof(indirizzoServer));
			nuovoIndirizzo.sin_family = AF_INET;
			nuovoIndirizzo.sin_port = htons(nuovaPorta);
			nuovoIndirizzo.sin_addr.s_addr = INADDR_ANY;
			printf("INF| Struttura nuovo indirizzo creata\n"); //INFO
			ret = bind(nuovoDescrSocket, (struct sockaddr*)&nuovoIndirizzo, sizeof(nuovoIndirizzo) );
			if(ret < 0){
        		perror("ERR| Errore in fase di bind");
       			exit(-1);
   			}

			//DESERIALIZZAZZIONE RRQ
			posizione=0;
			memcpy(&opcode,buffer,sizeof(opcode));
			opcode = ntohs(opcode);
			printf("INF| Il client(processo %d) ha inviato l'opcode: %d\n", getpid(), opcode); //INFO 
			posizione += sizeof(opcode);
			
			//MESSAGGIO NON RICONOSCIUTO
			if(opcode != RRQ){

				invioMessaggioErrore(&posizione, &opcode, nuovoDescrSocket, buffer, "Richiesta non valida.", RICHIESTANONVALIDA, ret, indirizzoClient);

				printf("ERR| Richiesta corrotta o non valida\n");
        		printf("INF| Fine connessione con il client(processo %d)\n\n", getpid());
       			exit(-1);
			}
			
			//MESSAGGIO RRQ
			strcpy(nomeFile, buffer+posizione);
			printf("INF| Il client(processo %d) ha inviato il nome file: %s\n", getpid(), nomeFile); //INFO 
			posizione += strlen(nomeFile)+2;
			strcat(percorsoLocale,nomeFile);
			strcpy(mode, buffer+posizione);
			printf("INF| Il client(processo %d) ha inviato la modalità: %s\n", getpid(), mode); //INFO 
			if(strcmp(mode, "octet")==0){
				strcpy(mode,"rb");
			}
			else{
				if(strcmp(mode, "netascii")==0){
					strcpy(mode,"r");
				}
				else{
					printf("ERR| Modo non riconosciuto");
					printf("INF| Fine connessione con il client(processo %d)\n\n", getpid());
					exit(-1);
				}
			}

			//APERTURA FILE ------------------------------------------------------
            close(descrSocket);
			
			//FILE NON TROVATO
			if ((puntFile = fopen(percorsoLocale, mode)) == NULL){
      			perror("ERR| Errore di apertura del file");
				
				invioMessaggioErrore(&posizione, &opcode, nuovoDescrSocket, buffer, "File non trovato.", FILENONTROVATO, ret, indirizzoClient);				
				
				printf("INF| Fine connessione con il client(processo %d)\n\n", getpid());
				exit(-1);
  			}

			//INVIO DATA 512byte ------------------------------------------------------
			numeroBlocco = 0;
   			while((numeroLetti = fread(buffer+sizeof(opcode)+sizeof(numBlockMsg), 1, DATA_SIZE, puntFile)) == DATA_SIZE){
				//fseek(puntFile, 0, SEEK_CUR); 
   				//printf("\nDEBUG| Letti %d byte: %s\n",numeroLetti, buffer+sizeof(opcode)+sizeof(numBlockMsg)); //DEBUG

				serializzaData(&posizione, &opcode, buffer, &numBlockMsg, numeroBlocco);
				
				ret = sendto(nuovoDescrSocket, (void*) buffer, BUF_SIZE, 0, (struct sockaddr*)&indirizzoClient, sizeof(indirizzoClient));
				if(ret < 0){
                    perror("ERR| Errore in fase di invio");
                    printf("INF| Fine connessione con il client(processo %d)\n\n", getpid());
                    exit(-1); 
                }

				//RICEZIONE ACK ------------------------------------------------------
				ricezioneACK(&posizione, &opcode, nuovoDescrSocket, buffer, &numACKmsg, &numeroACK, numeroBlocco, ret, indirizzoClient);

				memset(&buffer, 0, BUF_SIZE);
				numeroBlocco++;
				//sleep(1);
			}
			
			//INVIO ULTIMO DATA ------------------------------------------------------
			serializzaData(&posizione, &opcode, buffer, &numBlockMsg, numeroBlocco);

			//printf("\nDEBUG| Letti %d byte: %s\n",numeroLetti, buffer+sizeof(opcode)+sizeof(numBlockMsg)); //DEBUG
			ret = sendto(nuovoDescrSocket, (void*) buffer, numeroLetti+posizione, 0, (struct sockaddr*)&indirizzoClient, sizeof(indirizzoClient));
			if(ret < 0){
               	perror("ERR| Errore in fase di invio");
               	printf("INF| Fine connessione con il client(processo %d)\n\n", getpid());
               	exit(-1);
            }

			//ULTIMO ACK ------------------------------------------------------
			ricezioneACK(&posizione, &opcode, nuovoDescrSocket, buffer, &numACKmsg, &numeroACK, numeroBlocco, ret, indirizzoClient);

			memset(&buffer, 0, BUF_SIZE);
   			fclose(puntFile);

            close(nuovoDescrSocket);
            printf("INF| Fine connessione con il client(processo %d)\n\n", getpid()); //INFO 
            exit(1);

        } 

        //PADRE ------------------------------------------------------
        else {

        }
    }
}

////////////////////////////////////////////////////////////////////////////////    
///                    Made by: STEFANO PETROCCHI                            ///
////////////////////////////////////////////////////////////////////////////////    
