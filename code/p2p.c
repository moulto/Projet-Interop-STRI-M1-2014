
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <dirent.h>
#include <pthread.h>

#include "p2p.h"

#define TRUE 1
#define FALSE 0
#define LONGUEUR_TAMPON 4096


#include <dirent.h>

#include <errno.h>


#ifdef WIN32
#define perror(x) printf("%s : code d'erreur : %d\n", (x), WSAGetLastError())
#define close closesocket
#define socklen_t int
#endif


/* Variables cachees */

/* le socket d'ecoute */
int socketEcoute;
/* longueur de l'adresse */
socklen_t longeurAdr;

/* le socket client */
int socketClient;


/****************************************************************
               PARTIE CLIENT
*****************************************************************/

/* Initialisation.
 * Connexion au serveur sur la machine donnee et au service donne.
 * Utiliser localhost pour un fonctionnement local.
 */
int InitialisationAvecServiceModeClient(char *machine, char *service) {
	int n; /* Variable permettant de tester le retour de la fonction getaddrinfo() */
	struct addrinfo	hints, *res, *ressave; /*Variables permettant la creation et l'initialisation du socket */

/* On met les valeurs du mode connecte dans les variables */
	bzero(&hints, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/* On recupere les informations avec la fonction getaddrinfo() */
	if ( (n = getaddrinfo(machine, service, &hints, &res)) != 0)  {
			/* En cas d'erreur, on informe l'utilisateur */
     		fprintf(stderr, "Initialisation, erreur de getaddrinfo : %s", gai_strerror(n));
     		return 0;
	}

	/* On fait une sauvegarde des informations */
	ressave = res;

	/* On initialise le socket client */
	do {
		socketClient = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (socketClient < 0)
			continue;	/* ignore this one */

		/* On realise une demande de connexion sur le socket */
		if (connect(socketClient, res->ai_addr, res->ai_addrlen) == 0)
			break;		/* success */

		close(socketClient);	/* ignore this one */
	} while ( (res = res->ai_next) != NULL);

	if (res == NULL) {
     		perror("ERREUR : probleme de connexion au serveur\n");
     		return 0;
	}

	freeaddrinfo(ressave); /* Si tout est ok on libere la variable de sauvegarde */

	/* Success */
	return 1;
}

/* Recoit un message envoye par le serveur.
 */
char *ReceptionModeClient() {
	char message[LONGUEUR_TAMPON]; /* variable contenant le message reçu */
	unsigned short int messageLength; /* Variable contenant la taille du message */
	
	/* On met le tampon a zero */
	memset(message,0,sizeof message);

	/* On recupere le message du serveur */
	if(recv(socketClient,message,sizeof message,0) == -1){
		/* Si on recupere rien on retourne null */
		return NULL;
	}

	/* On retourne le message que l'on a reçu du serveur */
	return strdup(message);
}

/* Envoi un message au serveur.
 */
int EmissionModeClient(char *message) {
	/* On regarde si le message finit bien par \n */
	if(strstr(message, "\n") == NULL) {
		/* Si le message ne finit pas par \n, on le rajoute a la fin */
		strcat(message,"\n");
	}
	/* On recupere la taille du message */
	int taille = strlen(message);
	/* On envoie le message au serveur */
	if (send(socketClient, message, taille,0) == -1) {
        perror("Emission, probleme lors du send\n");
        return 0;
	}
	/* Success */
	return 1;
}

/* Recoit des donnees envoyees par le serveur.
 */
int ReceptionBinaireModeClient(char *donnees, size_t tailleMax) {
	int dejaRecu = 0;
	int retour = 0;
	/* si on n'est pas arrive au max
	 * on essaie de recevoir plus de donnees
	 */
	while(dejaRecu < tailleMax) {
		retour = recv(socketClient, donnees + dejaRecu, tailleMax - dejaRecu, 0);
		if(retour < 0) {
			perror("ReceptionBinaire, erreur de recv.");
			return -1;
		} else if(retour == 0) {
			fprintf(stderr, "ReceptionBinaire, le serveur a ferme la connexion.\n");
			return 0;
		} else {
			/*
			 * on a recu "retour" octets en plus
			 */
			dejaRecu += retour;
		}
	}
	return dejaRecu;
}


/* Ferme la connexion.
 */
void TerminaisonModeClient() {
	close(socketClient);
}

/* 
Connexion au serveur FTP.
retourne 1 si la connexion est OK et 0 sinon
*/
int connecterUtilisateurModeClient(){

	char *message = NULL; /* Variable qui va contenir les messages du serveur */
	char utilisateur[50]; /* Nom d'utilisateur avec lequel on veut se connecter */
	int erreur = 0; /* variable qui permet de tester la presence d'une erreur */
	char * requete; /* Requete que l'on va envoyer au serveur */

	/* On alloue de la memoire pour la variable requete */
	requete = (char*) malloc(60);

	/* On recupere le message du serveur */
	message = ReceptionModeClient();

	/* On affiche ce message */
	printf("%s",message);

	/* On va lire le nom d'utilisateur au clavier */
	fflush(stdin); /* On vide le tampon */
	fgets(utilisateur,50,stdin);

	/* On prepare la requete pour le serveur */
	strcat(requete,"USER ");
	strcat(requete,utilisateur);

	/* On envoie la requete au serveur */
	EmissionModeClient(requete);

	/* On recupere la reponse du serveur */
	message = NULL;
	message = ReceptionModeClient();

	/* On affiche cette reponse */
	printf("%s",message);

	/* On analyse la reponse du serveur pour voir la connexion est OK , il faut un message 230 */
	if(strstr(message,"230") != NULL){
		/* La reponse du serveur est bien de type 230 */
		return 1;
	}else{
		return 0;
	}
}

/* Envoi un fichier present dans le dossier courant sur le serveur */
void envoyerFichierModeClient(char *nomFichier){
	FILE * fichier = NULL; /* Fichier que l'on veut envoyer */
	char *contenuFichier; /* Contenu du fichier que l'on veut envoyer */
	char *reponseServeur; /* Reponse du serveur */
	char requete[100]; /* requete qu'on envoie au serveur */
	long taille; /* taille du fichier */

	/* On supprime le \n a la fin du nomFichier */
	nomFichier[strlen(nomFichier)-1] = 0;

	/* Ouverture du fichier en mode lecture */
	fichier = fopen(nomFichier,"rb");
	/* On teste l'ouverture du fichier */
	if(fichier == NULL){
		/* probleme ouverture */
		printf("ERREUR : ouverture du fichier echouee\n");
	}else{
		/* On recupere la taille du fichier */
		fseek (fichier , 0 , SEEK_END);
  		taille = ftell (fichier);
  		rewind (fichier);

  		/* On alloue de la memoire pour le contenu du fichier */
  		contenuFichier = malloc(taille);

		/* on va maintenant lire le contenu du fichier */
		if(fread(contenuFichier,taille*sizeof(char),1,fichier)<1){
			/* Erreur lecture fichier */
			/* On ferme le fichier */
			fclose(fichier);
			printf("ERREUR : lecture du fichier echouee\n");
		}else{
			/* On ferme le fichier */
			fclose(fichier);
			/* On teste que contenu fichier est non null */
			if(contenuFichier == NULL){
				/* Contenu fichier null */
				printf("ERREUR : contenu du fichier null\n");
			}else{
				/* on prepare la requete pour le serveur */
				sprintf(requete,"STOR %s\n",nomFichier);
				/* On envoie la requete au serveur */
				EmissionModeClient(requete);
				/* On affiche la reponse du serveur */
				reponseServeur = ReceptionModeClient();
				printf("%s",reponseServeur);
				/* Si la reponse est 150 - * on envoie le contenu */
				if(strstr(reponseServeur,"150") != NULL){
					/* Le serveur accepte la demande, on envoie le contenu */
					EmissionModeClient(contenuFichier);
					/* On recupere la reponse du serveur */
					reponseServeur = ReceptionModeClient();
					printf("%s",reponseServeur);	
				}
			}
		}
	}
}

/* Telecharge un fichier depuis le serveur */
void telechargerFichierModeClient(char *nomFichier){
	FILE * fichier = NULL; /* Fichier que l'on veut creer */
	char *contenuFichier; /* Contenu du fichier que l'on veut creer */
	char *reponseServeur; /* Reponse du serveur */
	char requete[100]; /* requete qu'on envoie au serveur */
	long taille; /* taille du fichier */

	/* On supprime le \n a la fin du nomFichier */
	nomFichier[strlen(nomFichier)-1] = 0;

	/* On verifie que le nom du fichier est non null ou vide */
	if(nomFichier == NULL || strcmp(nomFichier,"") == 0){
		/* Fichier null ou vide */
		printf("ERREUR : le nom du fichier est vide\n");
	}else{
		/* On prepare la requete pour le serveur */
		sprintf(requete,"RETR %s\n",nomFichier);
		/* On envoie la requete */
		EmissionModeClient(requete);
		/* On affiche la reponse du serveur */
		reponseServeur = ReceptionModeClient();
		printf("%s",reponseServeur);
		/* Si la reponse est 150 - * on recupere le contenu le contenu */
		if(strstr(reponseServeur,"150") != NULL){
			/* On recupere le contenu du fichier */
			contenuFichier = ReceptionModeClient();
			/* On teste que le contenu est non null */
			if(contenuFichier == NULL){
				/* Erreur avec le transfert du contenu */
				printf("ERREUR : Contenu du fichier NULL\n");
			}else{
				/* On recupere le message du serveur et on l'affiche */
				/* On va maintenant creer le fichier en local */
				/* Ouverture du fichier en mode lecture */
				fichier = fopen(nomFichier,"wb");
				/* On teste l'ouverture du fichier */
				if(fichier == NULL){
					/* probleme ouverture */
					printf("ERREUR : ouverture du fichier echouee\n");
				}else{
					/* On ecrit le contenu du fichier telecharge dans le fichier local */
					if(fwrite(contenuFichier,strlen(contenuFichier),1,fichier) < 1){
				    	/* Erreur ecriture du fichier */
				    	/* On ferme le fichier */
						fclose(fichier);
				        printf("ERREUR : ecriture du contenu du fichier echouee\n");
				        /* On informe le serveur que le fichier est bien cree */
				    	EmissionModeClient("KO\n");
				    }else{
				    	/* On ferme le fichier */
				    	fclose(fichier);
				    	/* On informe le serveur que le fichier est bien cree */
				    	EmissionModeClient("OK\n");
				    	/* On recupere la reponse du serveur et on l'affiche*/
				    	reponseServeur = ReceptionModeClient();
				    	printf("%s",reponseServeur);
				    }
				}
			}
		}
	}
}

/* Telecharge un fichier depuis le serveur */
int telechargerFichierBlocModeClient(char *nomFichier){
	FILE * fichier = NULL; /* Fichier que l'on veut creer */
	char *reponseServeur; /* Reponse du serveur */
	char requete[100]; /* requete qu'on envoie au serveur */
	char bloc[65536]; /* Bloc de fichier reçu du serveur, taille max des donnees + 3 octets d'en-tete */
	char *donneesBloc; /* Donnees contenues dans le bloc */
	char descripteurBloc[3]; /* champ descripteur de l'en-tete*/
	char tailleBloc[4]; /* champ taille de l'en-tete */
	int i; /* indice de parcours */

	/* On supprime le \n a la fin de nomFichier */
	nomFichier[strlen(nomFichier)-1] = 0;

	/* On verifie si le nom du fichier est NULL ou vide */
	if(nomFichier == NULL || strcmp(nomFichier,"") == 0){
		/* Fichier null ou vide */
		printf("ERREUR : le nom du fichier est vide\n");
		return 0;
	}else{
		/* On prepare la requete pour le serveur */
		sprintf(requete,"RETR %s\n",nomFichier);
		/* On envoie la requete */
		EmissionModeClient(requete);
		/* On affiche la reponse du serveur */
		reponseServeur = ReceptionModeClient();
		printf("%s",reponseServeur);
		/* Si la reponse est 150 - * on recupere le contenu */
		if(strstr(reponseServeur,"150") != NULL){
			/* On ouvre le fichier dans lequel on va ecrire */
			fichier = fopen(nomFichier,"wb");
			/* On teste l'ouverture du fichier */
			if(fichier == NULL){
				/* probleme ouverture */
				printf("ERREUR : ouverture du fichier echouee\n");
				/* On informe le serveur du probleme */
				EmissionModeClient("KO\n");
				/* On sort de la fonction en echec */
				return 0;
			}else{
				/* On va recuperer les blocs les uns apres les autres jusqu'a recevoir un descripteur = 64 */
				do{
					/* On recupere l'en-tete du bloc */
					ReceptionBinaireModeClient(bloc,3);
					
					/* On regarde si le descripteur est 0 */
					if(bloc[0] == 0){
						/* On recupere les donnees */
						unsigned short taille;
						memcpy(&taille,bloc+1,2);
						taille = ntohs(taille);
						ReceptionBinaireModeClient(bloc,taille);
						/* On va ecrire les donnees dans le fichier */
						if(fwrite(bloc,taille,1,fichier) < 1){
							/* Erreur ecriture du fichier */
					        printf("ERREUR : ecriture du contenu du fichier echouee\n");
					        /* On informe le serveur que le fichier est bien cree */
					    	EmissionModeClient("KO\n");
					    	/* On ferme le fichier */
					    	fclose(fichier);
					    	return 0;
						}else{
							/* On informe le serveur qu'on a bien reçu le bloc */
							EmissionModeClient("OK\n");
						}
						/* On libere l'espace alloue aux donnees */
						//memset(donneesBloc,0,sizeof(donneesBloc));
						//free(donneesBloc);
					}
				}while(bloc[0] != 64);

				/* on ferme le fichier et on informe le serveur que c'est OK */
				fclose(fichier);
				EmissionModeClient("OK\n");
				/* On affiche le message du serveur */
				printf("%s\n",ReceptionModeClient());
				return 1;
			}
		}
	}
}

/* Envoi au serveur une demande de changement du mode de telechargement des fichiers */
void changerModeModeClient(char mode){
	char requete[7]; /* Requete que l'on va envoyer au serveur */
	/* On prepare la requete */
	sprintf(requete,"MODE %c\n",mode);
	/* On envoie la requete au serveur */
	EmissionModeClient(requete);
	/* On affiche la reponse du serveur */
	puts(ReceptionModeClient());
}

/* Permet de reprendre un telechargement en cours en cas d'erreur */
int repriseTelechargementModeClient(char *nomFichier){
	FILE *fichier = NULL; /* fichier que l'on veut reprendre */
	long taille; /* taille actuelle du fichier */
	char *reponseServeur; /* Reponse du serveur */
	char requete[100]; /* requete qu'on envoie au serveur */
	char *bloc; /* Bloc de fichier reçu du serveur, taille max des donnees + 3 octets d'en-tete */
	char *donneesBloc; /* Donnees contenues dans le bloc */
	char descripteurBloc[3]; /* champ descripteur de l'en-tete */
	char tailleBloc[4]; /* champ taille de l'en-tete */
	int i; /* indice de parcours */

	/* On supprime le \n a la fin du nomFichier */
	nomFichier[strlen(nomFichier)-1] = 0;

	/* On verifie que le nom du fichier est non null ou vide */
	if(nomFichier == NULL || strcmp(nomFichier,"") == 0){
		/* Fichier null ou vide */
		printf("ERREUR : le nom du fichier est vide\n");
		return 0;
	}else{
		/* On ouvre le fichier en mode ajout */
		fichier = fopen(nomFichier,"ab");
		if(fichier == NULL){
			/* probleme ouverture */
			printf("ERREUR : ouverture du fichier echouee\n");
			/* On informe le serveur du probleme */
			EmissionModeClient("KO\n");
			/* On sort de la fonction en echec */
			return 0;
		}else{
			/* On recupere la taille du fichier */
			fseek (fichier , 0 , SEEK_END);
			taille = ftell (fichier);
			rewind (fichier);

			/* On prepare la requete pour le serveur */
			sprintf(requete,"REST %ld\n",taille);
			/* On envoie la requete au serveur */
			EmissionModeClient(requete);
			/* On recupere la reponse serveur et on regarde si elle contient 150 */
			reponseServeur = ReceptionModeClient();
			printf("%s",reponseServeur);
			if(strstr(reponseServeur,"150") != NULL){
				/* On va maintenant realiser la meme fonction que pour le mode bloc */
				/* On va recuperer les blocs les uns apres les autres jusqu'a recevoir un descripteur = 64 */
				do{
					bloc = ReceptionModeClient();
					//printf("Bloc : %s",bloc);
					/* On va dans un premier temps extraire les differents champs d'en-tete du bloc */
					sprintf(descripteurBloc,"%c%c%c",bloc[0],bloc[1],bloc[2]);
					sprintf(tailleBloc,"%c%c%c%c",bloc[3],bloc[4],bloc[5],bloc[6]);
					/* On regarde si le descripteur est 0 */
					if(atoi(descripteurBloc) == 0 || atoi(descripteurBloc) == 16){
						if(atoi(descripteurBloc) == 16){
							printf("Debut de la reprise\n");
						}
						/* On recupere les donnees */
						donneesBloc = (char*) malloc(atoi(tailleBloc));
						for(i=7;i<strlen(bloc)-1;i++){
							donneesBloc[i-7] = bloc[i];
						}
						/* On va ecrire les donnees dans le fichier */
						if(fwrite(donneesBloc,atoi(tailleBloc),1,fichier) < 1){
							/* Erreur ecriture du fichier */
					        printf("ERREUR : ecriture du contenu du fichier echouee\n");
					        /* On informe le serveur que le fichier est bien cree */
					    	EmissionModeClient("KO\n");
					    	/* On ferme le fichier */
					    	fclose(fichier);
					    	return 0;
						}else{
							/* On informe le serveur qu'on a bien reçu le bloc */
							EmissionModeClient("OK\n");
						}
						/* On libere l'espace alloue aux donnees */
						//memset(donneesBloc,0,sizeof(donneesBloc));
						//free(donneesBloc);
					}
				}while(atoi(descripteurBloc) != 64);

				/* on ferme le fichier et on informe le serveur que c'est OK */
				fclose(fichier);
				EmissionModeClient("OK\n");
				/* On affiche le message du serveur */
				printf("%s\n",ReceptionModeClient());
				return 1;
			}
		}

		
	}

	
}

/* Thread pour le telechargement parallele de fichiers en mode bloc */
void *telechargerFichierBlocThreadModeClient(void* param){
	char requete[100]; /* requete pour le serveur */
	char *reponseServeur; /* reponse du serveur */
	int tailleFichier; /* taille du fichier */
	int tailleParServeur; /* taille du fichier divisee par le nombre de serveur */
	FILE * fichier = NULL; /* Fichier que l'on veut creer */
	char bloc[65536]; /* Bloc de fichier reçu du serveur, taille max des donnees + 3 octets d'en-tete */
	char *donneesBloc; /* Donnees contenues dans le bloc */
	char descripteurBloc[3]; /* champ descripteur de l'en-tete*/
	char tailleBloc[4]; /* champ taille de l'en-tete */
	int i; /* indice de parcours */
	char nomFichier[100]; /* nom du fichier */

	donneesThread *donnees = (donneesThread *) param;

	/* on supprime le \n a la fin du nom de fichier */
	if(donnees->nomFichier[strlen(donnees->nomFichier)-1] == '\n'){
		donnees->nomFichier[strlen(donnees->nomFichier)-1] = 0;
	}

	/* connexion au serveur sauf pour le premier serveur  */
	if(donnees->numeroServeur != 1){	
		InitialisationAvecServiceModeClient("localhost",donnees->numPort);
		reponseServeur = ReceptionModeClient();
		printf("%s",reponseServeur);
		EmissionModeClient("USER temp\n");
		reponseServeur = ReceptionModeClient();
		printf("%s",reponseServeur);
	}
	/* demande taille */
	sprintf(requete,"SIZE %s\n",donnees->nomFichier);
	EmissionModeClient(requete);
	/* On recupere la reponse serveur */
	reponseServeur = ReceptionModeClient();
	/* On affiche la réception */
	printf("Message serveur %d : %s\n",donnees->numeroServeur,reponseServeur);
	/* On regarde si la reponse du serveur est bien de type 213 */
	if(strstr(reponseServeur,"213") != NULL){
		/* On recupere la taille du fichier */
		sscanf(reponseServeur,"213 %d",&tailleFichier);
		/* On regarde que la taille du fichier est pas egale a -1 */
		if(tailleFichier > 0){
			/* On divise la taille par le nombre de serveurs */
			tailleParServeur = tailleFichier / donnees->nombreServeurs;
			/* On prepare la requete pour le serveur du type REST debut fin */
			/* chaque serveur doit suivant la formule suivante : [(numero-1)*tailleParServeur]+1 jusqu'a numero*tailleParServeur sauf le dernier qui lit jusqu'a tailleFichier */
			if(donnees->numeroServeur == donnees->nombreServeurs){
				/* dernier serveur */
				sprintf(requete,"REST %s %d %d\n",donnees->nomFichier,((donnees->numeroServeur-1)*tailleParServeur)+1,tailleFichier);
			}else{
				/* on applique la formule */
				sprintf(requete,"REST %s %d %d\n",donnees->nomFichier,((donnees->numeroServeur-1)*tailleParServeur)+1,donnees->numeroServeur*tailleParServeur);
			}
			/* On envoi la requete */
			EmissionModeClient(requete);
			/* On affiche la reponse du serveur */
			reponseServeur = ReceptionModeClient();
			printf("%s",reponseServeur);
			/* Si la reponse est 150 - * on recupere le contenu */
			if(strstr(reponseServeur,"150") != NULL){
				/* On ouvre le fichier dans lequel on va ecrire */
				sprintf(nomFichier,"%s-part%d",donnees->nomFichier,donnees->numeroServeur);
				fichier = fopen(nomFichier,"wb");
				/* On teste l'ouverture du fichier */
				if(fichier == NULL){
					/* probleme ouverture */
					printf("ERREUR : ouverture du fichier echouee\n");
					/* On informe le serveur du probleme */
					EmissionModeClient("KO\n");
					/* On sort de la fonction en echec */
				}else{
					/* On va recuperer les blocs les uns apres les autres jusqu'a recevoir un descripteur = 64 */
					do{
						/* On recupere l'en-tete du bloc */
						ReceptionBinaireModeClient(bloc,3);
						
						/* On regarde si le descripteur est 0 */
						if(bloc[0] == 0){
							/* On recupere les donnees */
							unsigned short taille;
							memcpy(&taille,bloc+1,2);
							taille = ntohs(taille);
							ReceptionBinaireModeClient(bloc,taille);
							/* On va ecrire les donnees dans le fichier */
							if(fwrite(bloc,taille,1,fichier) < 1){
								/* Erreur ecriture du fichier */
						        printf("ERREUR : ecriture du contenu du fichier echouee\n");
						        /* On informe le serveur que le fichier est bien cree */
						    	EmissionModeClient("KO\n");
						    	/* On ferme le fichier */
						    	fclose(fichier);
							}else{
								/* On informe le serveur qu'on a bien reçu le bloc */
								EmissionModeClient("OK\n");
							}
							/* On libere l'espace alloue aux donnees */
							//memset(donneesBloc,0,sizeof(donneesBloc));
							//free(donneesBloc);
						}
					}while(bloc[0] != 64);

					/* on ferme le fichier et on informe le serveur que c'est OK */
					fclose(fichier);
					EmissionModeClient("OK\n");
					/* On affiche le message du serveur */
					printf("%s\n",ReceptionModeClient());
				}
			}
		}
	}
	pthread_exit(NULL);
}

/* Thread pour l'execution de la partie "client" du programme */
void *threadModeClient(void* param){
	int choix;
	int c; /* permet de vider le buffer */

	do{
		/* On affiche le menu */
		printf("Bienvenue, que voulez-vous faire :\n");
		printf("1 : Connexion\n");
		printf("0 : Quitter\n");
		printf("Votre choix : ");
		if(scanf("%d",&choix) < 1){
			printf("Probleme avec la saisie, merci de recommencer\n");
			choix = -1;
		}else{
			if(choix != 1 && choix != 0){
				/* Saisie incorrecte */
				printf("Erreur, votre saisie est incorrecte\n");
				while ( ((c = getchar()) != '\n') && c != EOF); /* on vide le buffer */
			}else{
				while ( ((c = getchar()) != '\n') && c != EOF); /* on vide le buffer */
				if(choix == 1){
					char numeroPort[10];
					printf("Indiquer le numero de port : \n");
					if(scanf("%s",numeroPort) < 1){
						printf("Erreur avec la saisie des informations du serveur\n");
						while ( ((c = getchar()) != '\n') && c != EOF); /* on vide le buffer */
					}else{
						while ( ((c = getchar()) != '\n') && c != EOF); /* on vide le buffer */
						char requete[100]; /* requete que l'on va envoyer au serveur */
						int etatConnexion; /* 1 : connecte / 0 : non connecte */
						char nomFichier[100]; /* Nom du fichier */
						int termine; /* permet de savoir si un client a termine ses traitements */
						int choixModeTransfert; /* Mode de transfert du fichier, 0 = bloc / 1 = flux */

						etatConnexion = 0;
						choixModeTransfert = 1; /* Mode flux par defaut */

						/* On initialise le client */
						if(InitialisationAvecServiceModeClient("localhost",numeroPort) == 0){
							/* Erreur sur l'initialisation, message d'erreur affiche par la fonction */
							pthread_exit(NULL);
						}

						/* On se connecte directement sur le serveur */
						etatConnexion = connecterUtilisateurModeClient();

						/* On verifie que l'utilisateur est bien connecte sur le serveur */
						if(etatConnexion == 1){
							/* On va maintenant afficher le menu principal de l'application */
							do{
								termine = FALSE;
								/* Menu principal */
								printf("-MENU PRINCIPAL-\n\n");
								printf("1 : Envoyer un fichier sur le serveur\n");
								printf("2 : Telecharger un fichier stocke sur le serveur\n");
								printf("3 : Modifier le mode de telechargement des fichiers (bloc / flux)\n");
								printf("4 : Reprendre un telechargement en cours (suite a une erreur)\n");
								printf("0 : Se deconnecter\n\n");
								printf("Votre choix : ");
								/* On recupere le choix de l'utilisateur */
								if(scanf("%d",&choix) < 1){
									/* Erreur saisie */
									printf("ERREUR : votre saisie est incorrecte \n\n");
									while ( ((c = getchar()) != '\n') && c != EOF); /* on vide le buffer */
								}else{
									while ( ((c = getchar()) != '\n') && c != EOF); /* on vide le buffer */
									/* On regarde le choix de l'utilisateur */
									switch (choix){
										case 1:
											/* Envoyer un fichier */
											printf("Saisir le nom du fichier que vous voulez envoyer (le fichier doit se trouver dans le repertoire actuel et le nom doit faire 100 caracteres max) : \n");
											/* On va lire le nom du fichier au clavier */
											fgets(nomFichier,100,stdin);
											fflush(stdin); /* on vide le buffer */
											/* On verifie qu'on a bien lu quelque chose */
											if(nomFichier != NULL){
												/* On lance la procedure d'envoi */
												envoyerFichierModeClient(nomFichier);
											}
											break;
										case 2:
											/* Telecharger un fichier */
											printf("Saisir le nom du fichier que vous voulez telecharger : \n");
											/* On va lire le nom du fichier au clavier */
											fgets(nomFichier,100,stdin);
											fflush(stdin); /* on vide le buffer */
											/* On verifie qu'on a bien lu quelque chose */
											if(nomFichier != NULL){
												/* En fonction du mode on appelle la fonction qui correspond */
												if(choixModeTransfert == 0){
														/* On telecharge normal depuis un seul serveur */
														telechargerFichierBlocModeClient(nomFichier);
												}else{
													/* On lance la procedure d'envoi */
													telechargerFichierModeClient(nomFichier);
												}
											}
											break;
										case 3:
											/* Modification du mode de transfert */
											do{
												printf("Quel mode de transfert voulez-vous choisir :\n");
												printf("1 : Mode bloc\n");
												printf("2 : Mode flux\n");
												printf("0 : Retour\n");
												printf("Votre choix : ");
												if(scanf("%d",&choixModeTransfert) < 1){
													/* Erreur saisie */
													printf("ERREUR : votre saisie est incorrecte \n\n");
													while ( ((c = getchar()) != '\n') && c != EOF); /* on vide le buffer */
												}else{
													while ( ((c = getchar()) != '\n') && c != EOF);
													/* On teste que l'utiliateur est bien saisi 1 ou 2 ou 0 */
													switch (choixModeTransfert){
														case 1: /* Passage en mode bloc */
															changerModeModeClient('B');
															choixModeTransfert = 0;
															break;
														case 2: /* Passage en mode flux */
															changerModeModeClient('S');
															choixModeTransfert = 1;
															break;
														default:
															/* Erreur saisie */
															printf("Votre choix est incorrect\n");
													}
												}
											}while(choixModeTransfert != 0 && choixModeTransfert != 1);
											break;
										case 4:
											printf("Saisir le nom du fichier que vous voulez reprendre : \n");
											/* On va lire le nom du fichier au clavier */
											fgets(nomFichier,100,stdin);
											fflush(stdin); /* on vide le buffer */
											/* On verifie qu'on a bien lu quelque chose */
											if(nomFichier != NULL){
												/* On passe en mode bloc */
												choixModeTransfert = 0;
												/* On reprend le transfert */
												repriseTelechargementModeClient(nomFichier);
											}
											break;
										case 0:
											/* Quitter l'application */
											EmissionModeClient("QUIT\n");
											printf("%s",ReceptionModeClient());
											TerminaisonModeClient();
											termine = TRUE;
											break;
										default:
											/* Erreur saisie */
											printf("ERREUR : votre saisie est incorrecte \n\n");
											break;
									}
								}

							}while(termine != TRUE);

						}
					}

				}
			}
		}
	}while(choix != 0);
	pthread_exit(NULL);
}

/************************************************
			MODE SERVEUR
***************************************************/


/* Initialisation.
 * Creation du serveur en precisant le service ou numero de port.
 * renvoie 1 si ca c'est bien passe 0 sinon
 */
int InitialisationAvecServiceModeServeur(char *service) {
	int n;
	const int on = 1;
	struct addrinfo	hints, *res, *ressave;

	#ifdef WIN32
	WSADATA	wsaData;
	if (WSAStartup(0x202,&wsaData) == SOCKET_ERROR)
	{
		//printf("WSAStartup() n'a pas fonctionne, erreur : %d\n", WSAGetLastError()) ;
		WSACleanup();
		exit(1);
	}
	memset(&hints, 0, sizeof(struct addrinfo));
    #else
	bzero(&hints, sizeof(struct addrinfo));
	#endif

	/* On initialise les variables en mode connecte */
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/* On recupere les informations avec getaddrinfo() */
	if ( (n = getaddrinfo(NULL, service, &hints, &res)) != 0)  {
			/* En cas d'echec on informe l'utilisateur */
     		//printf("Initialisation, erreur de getaddrinfo : %s", gai_strerror(n));
     		return 0;
	}
	/* On realise une sauvegarde des informations */
	ressave = res;

	/* On cree le socket d'ecoute pour les clients */
	do {
		socketEcoute = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (socketEcoute < 0)
			continue;		/* error, try next one */

		setsockopt(socketEcoute, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));
#ifdef BSD
		setsockopt(socketEcoute, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
#endif
		/* On associe le socket au serveur */
		if (bind(socketEcoute, res->ai_addr, res->ai_addrlen) == 0)
			break;			/* success */

		close(socketEcoute);	/* bind error, close and try next one */
	} while ( (res = res->ai_next) != NULL);

	if (res == NULL) {
     		perror("Initialisation, erreur de bind.");
     		return 0;
	}

	/* conserve la longueur de l'adresse */
	longeurAdr = res->ai_addrlen;

	/* On libere la sauvegarde */
	freeaddrinfo(ressave);
	/* On cree une file d'attente de maximum 5 clients */
	listen(socketEcoute, 5);
	/* Success */
	return 1;
}

/* Attends qu'un client se connecte, quand un client se connecte on lance un processus fils pour le traitement de sa requete */
Client * AttenteClientModeServeur() {
	struct sockaddr *clientAddr;
	char machine[NI_MAXHOST]; 
	Client *client; /* Client qui se connecte /

	/* On cree la structure client et on lui alloue de la memoire */
	client = (Client*) malloc(sizeof(Client));
	clientAddr = (struct sockaddr*) malloc(longeurAdr);

	/* On connecte le client au socket*/
	client->socketService = accept(socketEcoute, clientAddr, &longeurAdr);

	/* On verifie que la connexion du client au socket s'est bien passee */
	if (client->socketService == -1) {
		perror("AttenteClient, erreur de accept.");
		free(clientAddr);
		free(client);
		return 0;
	}

	/* On recupere les informations sur le client qui se connecte */
	if(getnameinfo(clientAddr, longeurAdr, machine, NI_MAXHOST, NULL, 0, 0) == 0) {
		//printf("Client sur la machine d'adresse %s connecte.\n", machine);
	} else {
		//printf("Client anonyme connecte.\n");
	}

	/* On libere la memoire allouee a l'adresse client */
	free(clientAddr);

	/*
	 * Reinitialisation buffer
	 */
	client->debutTampon = 0;
	client->finTampon = 0;	
	return client;
}

/* Recoit un message envoye par le serveur.
 */
char *ReceptionModeServeur(Client *client) {
	char message[LONGUEUR_TAMPON]; /* Message recu du client */
	unsigned short int messageLength; /* Variable contenant la taille du message */
	
	/* On remplit message de zero pour connaitre la fin */
	memset(message,0,sizeof message);

	/* On boucle en attendant de recevoir le message du client */
	while(recv(client->socketService,message,sizeof message,0) > 0){
		messageLength = strlen(message); /* On recupere la longueur du message */
		if(messageLength > 0){
			/* Si le message contient au moins 1 caractere on le retourne */
			return strdup(message);
		}
	}
}

/* Envoie un message au client.
 */
int EmissionModeServeur(char *message, Client *client) {
	int taille; /* Longueur du message */

	/* On verifie que le message se termine bien par \n */
	if(strstr(message, "\n") == NULL) {
		/* Si le message ne finit pas par \n, on le rajoute a la fin */
		strcat(message,"\n");
	}

	/* On recupere la taille du message */
	taille = strlen(message);

	/* On envoie le message au client */
	if (send(client->socketService, message, taille,0) == -1) {
        perror("Emission, probleme lors du send.\n");
        return 0;
	}

	return 1;
}

/* Envoie des donnees au client en precisant leur taille.
 */
int EmissionBinaireModeServeur(char *donnees, size_t taille, Client *client) {
	int retour = 0;
	retour = send(client->socketService, donnees, taille, 0);
	if(retour == -1) {
		perror("Emission, probleme lors du send.");
		return -1;
	} else {
		return retour;
	}
}


/* Ferme la connexion avec le client.
 */
void TerminaisonClientModeServeur(Client *client) {
	close(client->socketService);
	free(client);
}

/* Arrete le serveur.
 */
void TerminaisonModeServeur() {
	close(socketEcoute);
}

/* Mets tous les caracteres d'une chaine en majuscule */
char * putMajusculeModeServeur(char *ch){
    int i; /* indice de parcour de la chaine */
    for(i=0;i<strlen(ch)-1;i++){
        /* pour chaque caractere de ch on le remplace par la minuscule correspondante */
        ch[i] = toupper(ch[i]);
    }
    return ch;
}

/*
Parametres : str : chaine principale / len : longueur de la sous-chaine / pos : debut de la sous-chaine  
Extrait la sous-chaine de longueur "len" a partir du caractere numero "pos" dans la chaine "str" 
*/
char *extraireSousChaineModeServeur(char *str, long len, long pos){
	long i; /* indice de parcours de la chaine */
	char sousChaine[len]; /* Sous-chaine que l'on va retourner */

	/* On se positionne sur le debut de la sous-chaine et on recupere les i caracteres */
	for(i=pos;i<(pos+len);i++){
		sousChaine[i-pos] = str[i];
	}

	/* On retourne la sous-chaine */
	return strdup(sousChaine);
}

/* Realise la connexion du client en parametre sur le serveur FTP 
retourne 1 si client connecte et 0 sinon*/
int connecterClientModeServeur(Client *client){
	char *message = NULL; /* Message du client */
	char * messageSave;
	char *requete = NULL; /* Requete client */
	char *utilisateur = NULL; /* Nom d'utilisateur du client */

	//printf("Demande de connexion client\n");

	/* On demande le nom d'utilisateur au client */
	//printf("On demande le nom d'utilisateur\n");
	EmissionModeServeur("220 - Saisir utilisateur (max 50 caracteres) : \n", client);
	/* On recupere la reponse du client qui doit etre sous forme USER utilisateur */
	message = ReceptionModeServeur(client);
	/* On alloue de la memoire a la variable de sauvegarde pour pouvoir sauvegarder le message */
	messageSave = (char*) malloc(60);
	/* On sauvegarde le message recu */
	strcpy(messageSave,message);
	/* On analyse le message du client pour voir s'il est conforme */
	/* On decoupe le message du client pour extraire les informations */
	requete = strtok(message, " \n");
	utilisateur = strtok(NULL," \n");
	/* On teste que la requete est bien USER */
	if(strcmp(requete,"USER") != 0){
		/* requete incorrecte */
		//printf("Requete incorrecte : mauvaise commande\n");
		EmissionModeServeur("500 - Requete incorrecte\n",client);
		return 0;
	}else{
		/* On teste que l'utilisateur n'est pas NULL */
		if(utilisateur == NULL || strcmp(utilisateur,"") == 0){
			//printf("Utilisateur NULL ou vide\n");
			EmissionModeServeur("501 - Utilisateur NULL ou vide\n",client);
			return 0;
		}else{
			/* On teste maintenant que la requete n'est pas trop longue */
			/* On cree une requete de test qui est de la bonne longueur */
			char *testLongueurMessage;
			/* On lui alloue de la memoire */
			testLongueurMessage = (char*) malloc(60);
			sprintf(testLongueurMessage,"USER %s\n",utilisateur);
			/* On teste maintenant si la requete que l'on a recu du client fait la bonne longueur */
			if(strlen(testLongueurMessage) != strlen(messageSave)){
				/* requete incorrecte */
				//printf("Requete incorrecte : probleme de longueur\n");
				EmissionModeServeur("500 - Requete incorrecte\n",client);
				return 0;
			}else{
				/* On autorise la connexion du client sur le serveur */
				//printf("Connexion autorisee pour l'utilisateur %s\n",utilisateur);
				EmissionModeServeur("230 : Connexion etablie\n",client);
				return 1;
			}
		}
	}
	
}

/* Permet au client d'envoyer un fichier sur le serveur, si le fichier est deja present sur le serveur on ecrase */
void recevoirFichierModeServeur(Client *client, char *requete){
	char *requeteSave; /* sauvegarde de la requete passee en parametre */
	char *nomFichier; /* nom du fichier envoye par l'utilisateur */
	char *commande; /* commande de l'utilisateur */
	char fichierSave[100]; /* sauvegarde du nom du fichier */

	/* On alloue de la memoire a la sauvegarde de la requete */
	requeteSave = (char*) malloc(100);
	/* On sauvegarde la requete */
	strcpy(requeteSave,requete);
	/* On decompose la requete client pour extraire le nom du fichier et la commande */
	commande = strtok(requete, " \n");
	nomFichier = strtok(NULL, " \n");
	/* on sauvegarde le nom du fichier */
	strcpy(fichierSave,nomFichier);

	/* On verifie que la commande est bien STOR */
	if(strcmp(commande,"STOR") != 0){
		/* On envoie l'erreur au client */
		//printf("Requete incorrecte : mauvaise commande\n");
		EmissionModeServeur("500 - Requete incorrecte\n",client);
	}else{
		/* On teste que le chemin du fichier pas vide */
		if(nomFichier == NULL || strcmp(nomFichier,"") == 0){
			/* Erreur pas de chemin pour le fichier */
			//printf("ERREUR : Le chemin du fichier est vide\n");
			EmissionModeServeur("501 - Chemin du fichier NULL ou vide\n",client);
		}else{
			/* On teste maintenant la longueur de la requete */
			/* On va donc comparer la requete sauvegardee a une requete que l'on monte pour le test */
			sprintf(requete,"STOR %s\n",nomFichier);
			if(strlen(requeteSave) != strlen(requete)){
				/* requete incorrecte */
				//printf("Requete incorrecte : probleme de longueur\n");
				EmissionModeServeur("500 - Requete incorrecte\n",client);
			}else{
				/* On demande maintenant le contenu du fichier au client */
				EmissionModeServeur("150 - Transfert autorise\n",client);
				char *contenuFichier = NULL;
				contenuFichier = ReceptionModeServeur(client);

				/* On teste que le contenu a bien ete recu */
				if(contenuFichier == NULL){
					//printf("ERREUR : probleme reception contenu fichier\n");
					EmissionModeServeur("451 - Erreur avec l'envoi du fichier\n",client);
				}else{
					/* On va maintenant cree le fichier */
					FILE * fichier = NULL; /* Fichier dans lequel on va ecrire */
					strcpy(nomFichier,fichierSave); /* on recupere la sauvegarde du nom du fichier */
					fichier = fopen(nomFichier,"wb");
					/* On teste la creation/ouverture du fichier */
					if(fichier == NULL){
						/* Probleme creation fichier */
						//printf("ERREUR : creation fichier KO\n");
						EmissionModeServeur("451 - Erreur avec la creation du fichier sur le serveur\n",client);
					}else{
						//printf("%s\n",contenuFichier);
						/* On va maintenant ecrire le contenu dans la fichier */
						if(fwrite(contenuFichier,strlen(contenuFichier),1,fichier) < 1){
				    		/* Erreur ecriture du fichier */
				        	//printf("ERREUR : ecriture fichier KO\n");
				        	/* on ferme le fichier */
				    		fclose(fichier);
				        	EmissionModeServeur("451 - Erreur avec l'ecriture dans le fichier sur le serveur\n",client);
				    	}else{
				    		/* Si tout c'est bien on informe l'utilisateur */
				    		//printf("Transfert du fichier OK\n");
				    		/* on ferme le fichier */
				    		fclose(fichier);
				    		EmissionModeServeur("226 - Transfert du fichier termine\n",client);
				    		
				    	}
					}
				}
			}
		}
	}

}

/* Permet au serveur d'envoyer un fichier a un client qui en fait la demande */
int envoyerFichierModeServeur(Client *client, char *requete){
	char *requeteSave; /* sauvegarde de la requete passee en parametre */
	char *nomFichier; /* nom du fichier a envoyer */
	char *commande; /* commande de l'utilisateur */
	FILE * fichier = NULL; /* fichier a envoyer */
	char *contenuFichier; /* Contenu du fichier que l'on veut envoyer */
	long taille; /* taille du fichier */
	char fichierSave[100]; /* sauvegarde du nom du fichier */
	char *resultatTelechargement; /* Ok ou KO en fonction du resultat du telechargement */

	/* On alloue de la memoire a la sauvegarde de la requete */
	requeteSave = (char*) malloc(100);

	/* On alloue de la memoire pour la variable */
	resultatTelechargement = (char*) malloc(5);

	/* On sauvegarde la requete */
	strcpy(requeteSave,requete);

	/* On decompose la requete client pour extraire le nom du fichier et la commande */
	commande = strtok(requete, " \n");
	nomFichier = strtok(NULL, " \n");

	/* on sauvegarde le nom du fichier */
	strcpy(fichierSave,nomFichier);

	/* On verifie que la commande est bien RETR */
	if(strcmp(commande,"RETR") != 0){
		/* On envoie l'erreur au client */
		//printf("Requete incorrecte : mauvaise commande\n");
		EmissionModeServeur("500 - Requete incorrecte\n",client);
		return 0;
	}else{
		/* On teste que le chemin du fichier pas vide */
		if(nomFichier == NULL || strcmp(nomFichier,"") == 0){
			/* Erreur pas de chemin pour le fichier */
			//printf("ERREUR : Le chemin du fichier est vide\n");
			EmissionModeServeur("501 - Chemin du fichier NULL ou vide\n",client);
			return 0;
		}else{
			/* On teste maintenant la longueur de la requete */
			/* On va donc comparer la requete sauvegardee a une requete que l'on monte pour le test */
			sprintf(requete,"RETR %s\n",nomFichier);
			if(strlen(requeteSave) != strlen(requete)){
				/* requete incorrecte */
				//printf("Requete incorrecte : probleme de longueur\n");
				EmissionModeServeur("500 - Requete incorrecte\n",client);
				return 0;
			}else{
				/* On va maintenant ouvrir le fichier demande */
				strcpy(nomFichier,fichierSave); /* on recupere la sauvegarde du nom du fichier */
				/* Ouverture du fichier en mode lecture */
				fichier = fopen(nomFichier,"rb");
				/* On teste l'ouverture du fichier */
				if(fichier == NULL){
					/* Erreur ouverture fichier */
					//printf("ERREUR : ouverture du fichier impossible\n");
					EmissionModeServeur("550 - Impossible d'ouvrir le fichier\n",client);
					return 0;
				}else{
					/* on recupere le contenu du fichier */
					/* On recupere la taille du fichier */
					fseek (fichier , 0 , SEEK_END);
			  		taille = ftell (fichier);
			  		rewind (fichier);
					/* On alloue de la memoire pour le contenu du fichier */
					contenuFichier = (char*) malloc(taille);
					/* on va maintenant lire le contenu du fichier */
					if(fread(contenuFichier,1,taille,fichier)<1){
						/* Erreur lecture fichier */
						//printf("ERREUR : lecture du fichier echouee\n");
						/* on ferme le fichier */
				    	fclose(fichier);
						EmissionModeServeur("550 - Impossible de lire le fichier\n",client);
						return 0;
					}else{
						/* On teste que contenu fichier est non null */
						if(contenuFichier == NULL){
							/* Contenu fichier null */
							//printf("ERREUR : contenu du fichier null\n");
							/* on ferme le fichier */
				    		fclose(fichier);
							EmissionModeServeur("550 - Impossible de lire le fichier\n",client);
							return 0;
						}else{
							/* on ferme le fichier */
				    		fclose(fichier);
							/* On informe que le telechargement va debuter */
							EmissionModeServeur("150 - Debut du telechargement\n",client);
							/* On envoie le contenu du fichier au client */
							//printf("Envoi du contenu fichier\n");
							EmissionModeServeur(contenuFichier,client);
							//printf("Telechargement OK\n");

							/* On recupere la reponse avec le resultat du telechargement */
							resultatTelechargement = ReceptionModeServeur(client);
							if(strstr(resultatTelechargement,"OK") != NULL){
								/* On envoie le message 226 */
								EmissionModeServeur("226 - Telechargement termine\n",client);
							}else{
								/* On envoie 451 */
								EmissionModeServeur("451 - Telechargement echoue\n",client);
							}
							return 1;
						}
					}
				}
			}
		}
	}
}


/* Envoi en mode bloc, retourne 1 si ok et 0 sinon */
int envoyerFichierBlocModeServeur(Client *client, char *requete){
	FILE * fichier; /* Fichier que l'on veut envoyer */
	char *nomFichier; /* Chemin du fichier que l'on veut envoyer */
	char *requeteSave; /* Sauvegarde de la requete client pour le test de longueur */
	char fichierSave[100]; /* Sauvegarde du nom du fichier car il s'efface au cours de l'execution */
	char *commande; /* commande de l'utilisateur */
	char caractereCourrant; /* caractere lu dans le fichier */
	int compteur; /* compteur de caractere */
	char donnees[8191]; /* donnees du bloc */
	char *retourClient; /* reponse du client apres envoi d'un bloc */
	char *tailleDejaRecue; /* Depart de la reprise */

	/* On alloue de la memoire a la sauvegarde de la requete */
	requeteSave = (char*) malloc(100);

	/* On sauvegarde la requete */
	strcpy(requeteSave,requete);

	/* On decompose la requete client pour extraire le nom du fichier et la commande */
	commande = strtok(requete, " \n");
	nomFichier = strtok(NULL, " \n");

	/* on sauvegarde le nom du fichier */
	strcpy(fichierSave,nomFichier);

	/* On verifie que la commande est bien RETR */
	if(strcmp(commande,"RETR") != 0){
		/* On envoie l'erreur au client */
		//printf("Requete incorrecte : mauvaise commande\n");
		EmissionModeServeur("500 - Requete incorrecte\n",client);
		return 0;
	}else{
		/* On teste que le chemin du fichier pas vide */
		if(nomFichier == NULL || strcmp(nomFichier,"") == 0){
			/* Erreur pas de chemin pour le fichier */
			//printf("ERREUR : Le chemin du fichier est vide\n");
			EmissionModeServeur("501 - Chemin du fichier NULL ou vide\n",client);
			return 0;
		}else{
			/* On teste maintenant la longueur de la requete */
			/* On va donc comparer la requete sauvegardee a une requete que l'on monte pour le test */
			sprintf(requete,"RETR %s\n",nomFichier);
			if(strlen(requeteSave) != strlen(requete)){
				/* requete incorrecte */
				//printf("Requete incorrecte : probleme de longueur\n");
				EmissionModeServeur("500 - Requete incorrecte\n",client);
				return 0;
			}else{
				/* On recupere le nom du fichier de la sauvegarde */
				strcpy(nomFichier,fichierSave);
				/* On ouvre le fichier en mode binaire */
				fichier = NULL; /* on met fichier a NULL pour bien pouvoir tester l'ouverture */
				fichier = fopen(nomFichier,"rb");
				/* On teste l'ouverture du fichier */
				if(fichier == NULL){
					/* Echec de l'ouverture */
					//printf("ERREUR : ouverture du fichier impossible\n");
					EmissionModeServeur("550 - Impossible d'ouvrir le fichier\n",client);
					return 0;
				}else{
					/* On informe le client que le telechargement va commencer */
					EmissionModeServeur("150 - Debut du telechargement\n",client);
					/* On positionne le compteur de caractere a 0 */
					compteur = 0;
					/* On va lire le fichier caractere par caractere */
					do{
						/* On recupere le premier caractere */
						caractereCourrant = fgetc(fichier);
						/* Si on a atteint la fin du fichier, on prepare le bloc et on envoie */
						if(caractereCourrant == EOF){
							unsigned char descripteur = 0;
							EmissionBinaireModeServeur(&descripteur,1,client);
							unsigned short taille = htons(compteur); /* ntohs cote reception */
							EmissionBinaireModeServeur((char*)&taille,2,client);
							EmissionBinaireModeServeur(donnees,compteur,client);
							retourClient = ReceptionModeServeur(client);
							if(strstr(retourClient,"OK") == NULL){
								/* On regarde si le client demande la reprise */
								if(strstr(retourClient,"REST") != NULL){
									/* On recupere la taille deja recue */
									tailleDejaRecue = (char*) malloc(strlen(retourClient)-6);
									int x; /* indice de parcours */
									for(x=5;x<strlen(retourClient);x++){
										tailleDejaRecue[x-5] = retourClient[x];
									}
									/* On positionne le curseur dans le fichier a l'emplacement de la reprise */
									fseek(fichier,atoi(tailleDejaRecue),SEEK_SET);
								}else{
									//printf("Erreur client\n");
									return 0;
								}
								
							}
						}else{
							/* On ajoute le caractere a la partie des donnees du bloc */
							donnees[compteur] = caractereCourrant;
							/* On incremente le compteur */
							compteur++;
							/* Si le compteur atteint la taille du bloc, on prepare le bloc et on envoie le bloc */
							if(compteur == 8191){
								unsigned char descripteur = 0;
								EmissionBinaireModeServeur(&descripteur,1,client);
								unsigned short taille = htons(8191); /* ntohs cote reception */
								EmissionBinaireModeServeur((char*)&taille,2,client);
								EmissionBinaireModeServeur(donnees,8191,client);
								retourClient = ReceptionModeServeur(client);
								if(strstr(retourClient,"OK") == NULL){
									/* On regarde si le client demande la reprise */
									if(strstr(retourClient,"REST") != NULL){
										/* On recupere la taille deja recue */
										tailleDejaRecue = (char*) malloc(strlen(retourClient)-6);
										int x; /* indice de parcours */
										for(x=5;x<strlen(retourClient);x++){
											tailleDejaRecue[x-5] = retourClient[x];
										}
										/* On positionne le curseur dans le fichier a l'emplacement de la reprise */
										fseek(fichier,atoi(tailleDejaRecue),SEEK_SET);
									}else{
										//printf("Erreur client\n");
										return 0;
									}
									
								}
								/* On remet le compteur a 0 */
								compteur = 0;
							}
						}
					}while(caractereCourrant != EOF);
					unsigned char descripteur = 64;
					EmissionBinaireModeServeur(&descripteur,1,client);
					unsigned short taille = 0; /* ntohs cote reception */
					EmissionBinaireModeServeur((char*)&taille,2,client);
					/* On teste le retour client et on envoie le message correspondant */
					if(strstr(ReceptionModeServeur(client),"OK") != NULL){
						/* le retour client contient bien OK */
						//printf("Telechargement OK\n");
						EmissionModeServeur("226 - Telechargement termine\n",client);
					}else{
						//printf("Telechargement KO\n");
						EmissionModeServeur("451 - Telechargement echoue\n",client);
					}
					/* On quitte la fonction avec le code retour 1 */
					return 1;
				}
			}
		}
	}
}

/* Change le mode de transfert des fichier, retourne NULL si KO ou le codeMode si OK */
char changerModeModeServeur(char *requete, Client *client){
	char mode; /* code pour le mode demande */
	/* On recupere dans la requete le caractere correspondant au mode, si la requete est bien formee c'est le 6eme caractere, S = flux et B = bloc */
	mode = requete[5];
	/* On teste que le mode demande est bien correct */
	if(mode == 'B' || mode == 'S'){
		/* Le mode est correct on teste maintenant la longueur de la requete, typiquement MODE CODE\n => 7 caracteres */
		if(strlen(requete) == 7){
			//printf("%s\n",extraireSousChaineModeServeur(requete, 5, 0));
			/* Requete correcte, on teste maintenant la commande */
			if(strcmp(extraireSousChaineModeServeur(requete, 5, 0),"MODE ") == 0){
				/* La commande est correcte on retourne le nouveau mode */
				EmissionModeServeur("200 - Changement de mode de transfert termine\n",client);
				return mode;
			}else{
				/* Erreur commande incorrecte */
				EmissionModeServeur("500 - Commande inconnue\n",client);
				return 0;
			}
		}else{
			/* Longueur de requete incorrecte */
			EmissionModeServeur("500 - Erreur de syntaxe dans la requete, longueur incorrecte\n",client);
			return 0;
		}
	}else{
		/* Mode incorrect */
		EmissionModeServeur("501 - Le mode demande est incorrect\n",client);
		return 0;
	}
}

/* Renvoi au client la taille du fichier qu'il donne en parametre */
int tailleFichierModeServeur(char *requete, Client *client){
	FILE * fichier; /* Fichier que l'on veut envoyer */
	char *nomFichier; /* Chemin du fichier que l'on veut envoyer */
	char *requeteSave; /* Sauvegarde de la requete client pour le test de longueur */
	char fichierSave[100]; /* Sauvegarde du nom du fichier car il s'efface au cours de l'execution */
	char *commande; /* commande de l'utilisateur */
	long taille; /* taille du fichier */
	char reponse[100]; /* reponse pour le client */


	/* On alloue de la memoire a la sauvegarde de la requete */
	requeteSave = (char*) malloc(100);


	/* On sauvegarde la requete */
	strcpy(requeteSave,requete);

	/* On decompose la requete client pour extraire le nom du fichier et la commande */
	commande = strtok(requete, " \n");
	nomFichier = strtok(NULL, " \n");

	/* on sauvegarde le nom du fichier */
	strcpy(fichierSave,nomFichier);

	/* On verifie que la commande est bien SIZE */
	if(strcmp(commande,"SIZE") != 0){
		/* On envoie l'erreur au client */
		//printf("Requete incorrecte : mauvaise commande\n");
		EmissionModeServeur("500 - Requete incorrecte\n",client);
		return 0;
	}else{
		/* On teste que le chemin du fichier pas vide */
		if(nomFichier == NULL || strcmp(nomFichier,"") == 0){
			/* Erreur pas de chemin pour le fichier */
			//printf("ERREUR : Le chemin du fichier est vide\n");
			EmissionModeServeur("501 - Chemin du fichier NULL ou vide\n",client);
			return 0;
		}else{

			/* On teste maintenant la longueur de la requete */
			/* On va donc comparer la requete sauvegardee a une requete que l'on monte pour le test */
			sprintf(requete,"SIZE %s\n",nomFichier);
			if(strlen(requeteSave) != strlen(requete)){
				/* requete incorrecte */
				//printf("Requete incorrecte : probleme de longueur\n");
				EmissionModeServeur("500 - Requete incorrecte\n",client);
				return 0;
			}else{
	
				/* On recupere le nom du fichier de la sauvegarde */
				strcpy(nomFichier,fichierSave);
				/* On ouvre le fichier en mode binaire */
				fichier = NULL; /* on met fichier a NULL pour bien pouvoir tester l'ouverture */
				fichier = fopen(nomFichier,"rb");
				/* On teste l'ouverture du fichier */
				if(fichier == NULL){
					/* Echec de l'ouverture */
					//printf("ERREUR : ouverture du fichier impossible\n");
					EmissionModeServeur("550 - Impossible d'ouvrir le fichier\n",client);
					return 0;
				}else{
		
					/* On recupere la taille du fichier */
					fseek (fichier , 0 , SEEK_END);
			  		taille = ftell (fichier);
			  		rewind (fichier);
			  		/* On envoi la taille du fihier */
			  		sprintf(reponse,"213 %ld\n",taille);
			  		EmissionModeServeur(reponse,client);
			  		//printf("Taille du fichier envoyee\n");
			  		return 1;
				}
			}
		}
	}
}


/* Envoi une partie d'un fichier au client */
int envoyerPartieFichierModeServeur(Client *client, char *requete){
	FILE * fichier; /* Fichier que l'on veut envoyer */
	char nomFichier[100]; /* Chemin du fichier que l'on veut envoyer */
	char fichierSave[100]; /* Sauvegarde du nom du fichier car il s'efface au cours de l'execution */
	char commande[5]; /* commande de l'utilisateur */
	int debut; /* fin de la partie */
	int fin; /* debut de la partie */
	char *reponse; /* reponse pour le client */
	char requeteSave[500]; /* sauvegarde de la requete */
	char donnees[8191]; /* donnees du bloc */
	char caractereCourrant; /* caractere lu dans le fichier */

	strcpy(requeteSave,requete);

	/* on recupere les elements presents dans la requete */
	if(sscanf(requete,"%s %s %d %d",commande,nomFichier,&debut,&fin) < 1){
		/* Erreur dans lecture chaine */
		//printf("Erreur lecture chaine\n");
		EmissionModeServeur("451 - ERREUR decomposition de la requete echouee\n",client);
		return 0;
	}else{
		/* on sauvegarde le nom du fichier */
		strcpy(fichierSave,nomFichier);
		/* On verifie que la commande est bien REST */
		if(strcmp(commande,"REST") != 0){
			/* On envoie l'erreur au client */
			//printf("Requete incorrecte : mauvaise commande\n");
			EmissionModeServeur("500 - Requete incorrecte\n",client);
			return 0;
		}else{
			/* On teste que le chemin du fichier pas vide */
			if(nomFichier == NULL || strcmp(nomFichier,"") == 0){
				/* Erreur pas de chemin pour le fichier */
				//printf("ERREUR : Le chemin du fichier est vide\n");
				EmissionModeServeur("501 - Chemin du fichier NULL ou vide\n",client);
				return 0;
			}else{
				/* On teste maintenant la longueur de la requete */
				/* On va donc comparer la requete sauvegardee a une requete que l'on monte pour le test */
				sprintf(requete,"REST %s %d %d\n",nomFichier,debut, fin);
				if(strlen(requeteSave) != strlen(requete)){
					/* requete incorrecte */
					//printf("Requete incorrecte : probleme de longueur\n");
					EmissionModeServeur("500 - Requete incorrecte\n",client);
					return 0;
				}else{
					/* On recupere le nom du fichier de la sauvegarde */
					strcpy(nomFichier,fichierSave);
					/* On ouvre le fichier en mode binaire */
					fichier = NULL; /* on met fichier a NULL pour bien pouvoir tester l'ouverture */
					fichier = fopen(nomFichier,"rb");
					/* On teste l'ouverture du fichier */
					if(fichier == NULL){
						/* Echec de l'ouverture */
						//printf("ERREUR : ouverture du fichier impossible\n");
						EmissionModeServeur("550 - Impossible d'ouvrir le fichier\n",client);
						return 0;
					}else{
						int nbCaractereLus; /* compteur du nombre total de caracteres lus */
						int compteur; /* compteur pour le bloc */
						/* On informe le client que le telechargement va commencer */
						EmissionModeServeur("150 - Debut du telechargement\n",client);
						/* On positionne le compteur de caractere a 0 */
						compteur = 0;
						nbCaractereLus = 0;
						/* On se positionne au bon endroit dans le fichier */
						fseek(fichier,debut,SEEK_SET);
						/* On va lire le fichier caractere par caractere */
						do{
							/* On recupere le premier caractere */
							caractereCourrant = fgetc(fichier);
							/* Si on a atteint la fin du fichier, on prepare le bloc et on envoie */
							if(nbCaractereLus == fin){
								unsigned char descripteur = 0;
								EmissionBinaireModeServeur(&descripteur,1,client);
								unsigned short taille = htons(compteur); /* ntohs cote reception */
								EmissionBinaireModeServeur((char*)&taille,2,client);
								EmissionBinaireModeServeur(donnees,compteur,client);
								reponse = ReceptionModeServeur(client);
								if(strstr(reponse,"OK") == NULL){
									//printf("Erreur client\n");
									return 0;	
								}
							}else{
								/* On ajoute le caractere a la partie des donnees du bloc */
								donnees[compteur] = caractereCourrant;
								/* On incremente le compteur */
								compteur++;
								nbCaractereLus++;
								/* Si le compteur atteint la taille du bloc, on prepare le bloc et on envoie le bloc */
								if(compteur == 8191){
									unsigned char descripteur = 0;
									EmissionBinaireModeServeur(&descripteur,1,client);
									unsigned short taille = htons(8191); /* ntohs cote reception */
									EmissionBinaireModeServeur((char*)&taille,2,client);
									EmissionBinaireModeServeur(donnees,8191,client);
									reponse = ReceptionModeServeur(client);
									if(strstr(reponse,"OK") == NULL){
										//printf("Erreur client\n");
										return 0;	
									}
									/* On remet le compteur a 0 */
									compteur = 0;
								}
							}
						}while(caractereCourrant != EOF);
						unsigned char descripteur = 64;
						EmissionBinaireModeServeur(&descripteur,1,client);
						unsigned short taille = 0; /* ntohs cote reception */
						EmissionBinaireModeServeur((char*)&taille,2,client);
						/* On teste le retour client et on envoie le message correspondant */
						if(strstr(ReceptionModeServeur(client),"OK") != NULL){
							/* le retour client contient bien OK */
							//printf("Telechargement OK\n");
							EmissionModeServeur("226 - Telechargement termine\n",client);
						}else{
							//printf("Telechargement KO\n");
							EmissionModeServeur("451 - Telechargement echoue\n",client);
						}
						/* On quitte la fonction avec le code retour 1 */
						return 1;
					}
				}
			}
		}
	}
}

/* Thread pour l'execution de la partie serveur */
void *threadModeServeur(void* param){
	Client *client; /* Client connecte sur le serveur */
	int termine; /* variable qui permet de savoir si un client souhaite terminer sa session */
	char *requete; /* Requete du client */
	char modeTransfert; /* Mode de transfert des fichiers : S (flux) , B (bloc) */

	donneesServeur *donnees = (donneesServeur *) param;

	/* On initialise le serveur */
	InitialisationAvecServiceModeServeur(donnees->portEcoute);

	/* On boucle en attendant les connexions des clients */
	while(1) {
		/* On recupere le client qui s'est connecte au serveur */
		client = AttenteClientModeServeur();
		if(connecterClientModeServeur(client) == 1){
			/* Par defaut le mode de transfert est le mode flux */
			modeTransfert = 'S';
			/* On va realiser le traitement du client */
			do{
				termine = FALSE; /* On met termine a 0 (faux) */
				/* On recupere la requete du client */
				requete = ReceptionModeServeur(client);
				//printf("On a recu : %s",requete);
				/* On teste que la requete n'est pas vide */
				if(requete==NULL){
					/* Si la requete est nulle on informe l'utilisateur */
					//printf("ERREUR : requete nulle\n");
					EmissionModeServeur("ERREUR : requete nulle\n",client);
				}else{
					/* On regarde par quelle lettre commence la requete (S (stor) R (retr) Q (quit)) */
					if(requete[0] == 'S'){
						if(requete[1] == 'T'){
							/* Demande d'envoi de fichier */
							//printf("Demande d'envoi d'un fichier\n");
							recevoirFichierModeServeur(client,requete);
						}
						if(requete[1] == 'I'){
							/* Demande taille d'un fichier */
							//printf("Demande de la taille d'un fichier\n");
							tailleFichierModeServeur(requete,client);
						}	
					}else{
						if(requete[0] == 'R'){
							if(requete[2] == 'T'){
								/* Demande de telechargement d'un fichier */
								//printf("Demande de telechargement de fichier\n");
								/* on teste le mode de transfert en cours */
								if(modeTransfert == 'B'){
									/* Mode bloc */
									envoyerFichierBlocModeServeur(client, requete);
								}else{
									/* Mode flux */
									envoyerFichierModeServeur(client,requete);
								}
							}
							if(requete[2] == 'S'){
								/* Demande de telechargement d'une partie d'un fichier */
								//printf("Demande de telechargement d'une partie d'un fichier\n");
								envoyerPartieFichierModeServeur(client,requete);
							}

						}else{
							if(requete[0] == 'Q'){
								/* Demande de fin de session */
								termine = TRUE;
								//printf("Fin de session client\n");
								EmissionModeServeur("530 - Fin de connexion\n",client);
							}else{
								if(requete[0] == 'M'){
									/* Changement du mode de transfert */
									//printf("Changement de mode\n");
									modeTransfert = changerModeModeServeur(requete,client);
								}else{
									/* Requete inconnue */
									printf("ERREUR : requete inconnue\n");
									EmissionModeServeur("500 - Commande non reconnue\n",client);
									termine = TRUE;
								}
							}
						}
					}
				}
			}while(termine != TRUE);
		}
	}

	/* On libere le socket */
	Terminaison();
	pthread_exit(NULL);
}