#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "serveur.h"

#define TRUE 1
#define FALSE 0

int main(int argc, char *argv[]) {
	Client *client; /* Variable permettant de récupérer le client connecté sur le serveur */
	int termine; /* Variable qui permet de savoir si un client a terminé ses traitements */
	char *message = NULL; /* Variable pour les requêtes clients */
	char *commande = NULL; /* Commande FTP envoyer par un utilisateur */
	
	/* initialisation du serveur */
	if(InitialisationAvecService(argv[1]) == 1){
		/* Initialisation OK */
		printf("Initialisation du serveur -> OK \n");
	}else{
		/* Problème d'initialisation */
		return -1;
	}

	
	char * test =listeDir("/home/florian/Dropbox/STRI/cours/interop/projet1/fichiersServeur");
	if(test == NULL){
		printf("test = NULL\n");
		return -1;
	}
	printf("%s\n",test);

	/* On boucle pour avoir un service continu */
	while(1){
		/* On récupère le client qui se connecte sur le serveur*/
		client = AttenteClient();
		/* On met la variable de fin des traitements à faux */
		termine = FALSE;
		do{
			/* On vérifie que la variable message est bien positionnée sur NULL et sinon on la met à NULL */
			if(message != NULL){
				message = NULL;
			}
			/* On récupère la requête du client */
			message = Reception(client);
			/* On teste si on a reçu quelque chose */
			if(message != NULL && strlen(message) > 0){

				/* On a bien reçu la requête du client 
				 Requête sous forme comande#arg1#arg2... 
				 On récupère donc dans un premier temps la commande */

				 /* On regarde si commande est bien vide et sinon on le fou à NULL */
				 if(commande != NULL){
				 	commande = NULL;
				 }

				 /* On récupère la commande dans la variable commande */
				 if(sscanf(message,"%s#",commande) > 0){
				 	/* On met la commande en majuscule pour éviter les problèmes de casse */
				 	commande = putMajuscule(commande);
				 	printf("On a reçu la commande : %s\n", commande);
				 }
			}

		}while(termine != TRUE);
	}
	/* On libère le socket */
	Terminaison();
	return 0;
}