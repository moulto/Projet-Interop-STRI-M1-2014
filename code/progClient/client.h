#ifndef __CLIENT_H__
#define __CLIENT_H__

/* Structure de donnees pour les thread de telechargement */
typedef struct{
	/* data */
	char numPort[10];
	char nomFichier[100];
	int numeroServeur;
	int nombreServeurs;
}donneesThread;

/* Initialisation.
 * Connexion au serveur sur la machine donnee et au service donne.
 * Utiliser localhost pour un fonctionnement local.
 * renvoie 1 si ca c'est bien passe 0 sinon
 */
int InitialisationAvecService(char *machine, char *service);

/* Recoit un message envoye par le serveur.
 * retourne le message ou NULL en cas d'erreur.
 * Note : il faut liberer la memoire apres traitement.
 */
char *Reception();

/* Envoie un message au serveur.
 * Attention, le message doit etre termine par \n
 * renvoie 1 si �ca c'est bien passe 0 sinon
 */
int Emission(char *message);

/* Recoit des donnees envoyees par le serveur.
 * renvoie le nombre d'octets recus, 0 si la connexion est fermee,
 * un nombre negatif en cas d'erreur
 */
int ReceptionBinaire(char *donnees, size_t tailleMax);

/* Envoie des donnees au serveur en precisant leur taille.
 * renvoie le nombre d'octets envoyes, 0 si la connexion est fermee,
 * un nombre negatif en cas d'erreur
 */
int EmissionBinaire(char *donnees, size_t taille);

/* Ferme la connexion.
 */
void Terminaison();

/* Recupere la liste des fichiers dans le repertoire courant du serveur */
void listeFichiers();

/* 
Connexion au serveur FTP.
retourne 1 si la connexion est OK et 0 sinon
*/
int connecterUtilisateur();

/* Envoi un fichier present dans le dossier courant sur le serveur */
void envoyerFichier(char *nomFichier);

/* Telecharge un fichier depuis le serveur */
void telechargerFichier(char *nomFichier);

/* Telecharge un fichier depuis le serveur */
int telechargerFichierBloc(char *nomFichier);

/* Envoi au serveur une demande de changement du mode de telechargement des fichiers */
void changerMode(char mode);

/* Permet de reprendre un telechargement en cours en cas d'erreur */
int repriseTelechargement(char *nomFichier);

void *telechargerFichierBlocThread(void* param);

#endif
