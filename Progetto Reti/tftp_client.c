////////////////////////////////////////////////////////////////////////////////    
///                                CLIENT                                    ///
////////////////////////////////////////////////////////////////////////////////

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BUF_SIZE 516
#define INDIRIZZO_SERVER_DEFAULT "127.0.0.1"
#define PORTA_DEFAULT 69
#define MAX_LEN_MODE 8 
#define MAX_DIM_COMANDO 10 
#define RRQ 1
#define DATA 3
#define ACK 4
#define ERROR 5
#define DATA_SIZE 512
#define PORTA 2
#define INDIRIZZO 1
#define BIN 0
#define TXT 1

////////////////////////////////////////////////////////////////////////////////    
///                       STRUTTURE & VARIABILI                              ///
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]){
    int ret, retDati, descrSocket, datiPreliminari, posizione, riempimento, numeroBlocco, numeroScritti, mode;
    uint16_t opcode, numBlockMsg, porta, errore;
	socklen_t lunghezza;
    struct sockaddr_in indirizzoServer, nuovoIndirizzoServer;
    char buffer[BUF_SIZE], comando[MAX_DIM_COMANDO], nomeFile[BUF_SIZE], percorsoLocale[BUF_SIZE], parametro1[3], parametro2[3];
	FILE *puntFile;
	strcpy(comando,"");
	mode = BIN;
	strcpy(parametro1, "wb+");
	strcpy(parametro2, "ab+");
    
////////////////////////////////////////////////////////////////////////////////    
///                           COMANDI                                        ///
////////////////////////////////////////////////////////////////////////////////
	
	while(1){
		printf("\nInserire comando: ");
		scanf("%s", comando);
		
		//COMANDO !QUIT ------------------------------------------------------
		if(strcmp(comando,"!quit") == 0)
			exit(1);

		//COMANDO !HELP ------------------------------------------------------
		if(strcmp(comando,"!help") == 0){
			printf("\nSono disponibili i seguenti comandi:\n!help --> mostra l'elenco dei comandi disponibili\n!mode {txt|bin} --> imposta il modo di trasferimento dei files (testo o binario)\n!get filename nome_locale --> richiede al server il nome del file <filename> e lo salva localmente con il nome <nome_locale>\n!quit --> termina il client\n\nMade by: Stefano Petrocchi\n");
			continue;
		}

		//COMANDO !MODE ------------------------------------------------------
		if(strcmp(comando,"!mode") == 0){
			scanf("%s", comando);
			if(strcmp(comando,"bin") == 0){
				mode = BIN;
				strcpy(parametro1, "wb+");
				strcpy(parametro2, "ab+");
				printf("Modo di trasferimento binario configurato\n");
				continue;
			}
			if(strcmp(comando,"txt") == 0){
				mode = TXT;
				strcpy(parametro1, "w+");
				strcpy(parametro2, "a+");
				printf("Modo di trasferimento testuale configurato\n");
				continue;
			}
			printf("\nComando non riconosciuto\n");
			continue;
		}

		//COMANDO !GET ------------------------------------------------------
		if(strcmp(comando,"!get") == 0){

			datiPreliminari = 1;
			scanf("%s", nomeFile);
			scanf("%s", percorsoLocale);
			printf("Richiesta file %s al server in corso.\n", nomeFile);

////////////////////////////////////////////////////////////////////////////////    
///                         SOCKET & INDIRIZZO                               ///
////////////////////////////////////////////////////////////////////////////////
			
			descrSocket = socket(AF_INET, SOCK_DGRAM, 0);
			//printf("\nDEBUG| Socket UDP creato: %d\n", descrSocket); //DEBUG

			memset(&indirizzoServer, 0, sizeof(indirizzoServer)); 
			indirizzoServer.sin_family = AF_INET;
			if(argc==3)
				porta = atoi(argv[PORTA]);
			else
				porta = PORTA_DEFAULT;
			indirizzoServer.sin_port = htons(porta);
			if(argc>1)
				inet_pton(AF_INET, argv[INDIRIZZO] , &indirizzoServer.sin_addr);
			else
				inet_pton(AF_INET, INDIRIZZO_SERVER_DEFAULT , &indirizzoServer.sin_addr);
			//printf("DEBUG| Struttura indirizzo server inizializzata (IP:%s , porta: %d, argomenti passati: %d)\n", argv[INDIRIZZO], porta, argc); //DEBUG

////////////////////////////////////////////////////////////////////////////////    
///                         GESTIONE CONNESSIONE                             ///
////////////////////////////////////////////////////////////////////////////////
			
			while(1){

				//INVIO RRQ ------------------------------------------------------
				if(datiPreliminari){
					posizione=0;
					riempimento=0;
					opcode = htons(RRQ); 
					memcpy(buffer,&opcode,sizeof(opcode));
					posizione += sizeof(opcode);
					strcpy(buffer+posizione,nomeFile);
					posizione += strlen(nomeFile)+1;
					memcpy(buffer+posizione,&riempimento,1);
					posizione++;
					if(mode){
						strcpy(buffer+posizione,"netascii\0");
						posizione += strlen("netascii\0")+1;
					}
					else{
						strcpy(buffer+posizione,"octet\0");
						posizione += strlen("octet\0")+1;
					}
					memcpy(buffer+posizione,&riempimento,1);
					posizione++;

					ret = sendto(descrSocket, (void*) buffer, posizione, 0, (struct sockaddr*)&indirizzoServer, sizeof(indirizzoServer));
					if(ret < 0){
				    	perror("Errore in fase di invio");
				   		break;
			  		}
					//printf("DEBUG| Messaggio inviato con successo\n"); //DEBUG
				}

				//RICEZIONE DATI O MESSAGGIO DI ERRORE ------------------------------------------------------
				lunghezza = sizeof(nuovoIndirizzoServer);
				retDati = recvfrom(descrSocket, (void*)buffer, BUF_SIZE, 0, (struct sockaddr*)&nuovoIndirizzoServer, &lunghezza);
				if(retDati < 0){
				    perror("Errore in fase di ricezione");
				    break;
				}

				posizione=0;
				memcpy(&opcode,buffer,sizeof(opcode));
				opcode = ntohs(opcode);
				posizione += sizeof(opcode);
				//printf("\nDEBUG| Il server ha inviato l'opcode: %d\n", opcode); //DEBUG
				if(opcode != DATA){ 
					if(opcode == ERROR){// MESSAGGIO DI ERRORE
						memcpy(&errore, buffer + posizione, sizeof(errore));
						errore = ntohs(errore);
						posizione += sizeof(errore);
						strcpy(buffer, buffer+posizione);
						//printf("\nERRORE| Il server ha inviato l'errore numero %d: %s\n", errore, buffer); //DEBUG
						printf("%s\n", buffer);
						break;
					}
					printf("Opcode non riconosciuto.\n");
					break;
				}// DATI
				memcpy(&numBlockMsg,buffer + posizione,sizeof(numBlockMsg));
				numeroBlocco = ntohs(numBlockMsg);
				posizione += sizeof(numBlockMsg);
				//printf("DEBUG| Il server ha inviato il numero blocco: %d\n", numeroBlocco); //DEBUG 
				//printf("\nDEBUG| Letti %d byte: %s\n",retDati, buffer + posizione); //DEBUG

				//SCRITTURA FILE ------------------------------------------------------
				if(datiPreliminari){
					printf("Trasferimento file in corso.\n");
					if ((puntFile = fopen(percorsoLocale, parametro1)) == NULL){
			   			perror("Impossibile aprire il file");
						break;
		   			}
				}
				else{
					if ((puntFile = fopen(percorsoLocale, parametro2)) == NULL){
			   			perror("Impossibile aprire il file");
						break;
		   			}
				}
				datiPreliminari = 0;
		
			  	numeroScritti=fwrite(buffer + posizione, retDati - posizione, 1, puntFile);
				if (numeroScritti<0){
			   			printf("Errore in fase di scrittura");
						break;
		   		} 
				//printf("DEBUG| Scrittura avvenuta con successo\n"); //DEBUG
				fclose(puntFile);

				//INVIO ACK ------------------------------------------------------
				posizione=0;
				opcode = htons(ACK);
				memcpy(buffer,&opcode,sizeof(opcode));
				posizione += sizeof(opcode);
				numBlockMsg = htons(numeroBlocco);
				memcpy(buffer+posizione,&numBlockMsg,sizeof(numBlockMsg));
				posizione += sizeof(numBlockMsg);

				ret = sendto(descrSocket, (void*) buffer, posizione, 0, (struct sockaddr*)&nuovoIndirizzoServer, sizeof(nuovoIndirizzoServer));
				if(ret < 0){
				    perror("Errore in fase di invio");
				   	break;
			  	}
				//printf("DEBUG| ACK inviato con successo\n");

				if(retDati<BUF_SIZE){
					printf("Trasferimento completato (%d/%d blocchi)\n", numeroBlocco+1, numeroBlocco+1);
					printf("Salvataggio %s completato\n\n", percorsoLocale);
					break;
				}
				memset(&buffer, 0, BUF_SIZE);     
			}
		
			//CHIUSURA CONNESSIONE ------------------------------------------------------
			close(descrSocket);
			continue;
		}
		printf("Immettere un comando valido\n");
	}
}

////////////////////////////////////////////////////////////////////////////////    
///                    Made by: STEFANO PETROCCHI                            ///
////////////////////////////////////////////////////////////////////////////////    
