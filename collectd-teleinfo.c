// teleinfo.c
// Lecture données Téléinfo par le port série du Wrt54gl (ou PC) et enregistre données dans fichier csv.
// Vérification checksum et boucle de 3 essais si erreurs.
// Par domos78 at free point fr
 
/*
Paramètres à adapter: 
- Port série à modifier en conséquence avec SERIALPORT.
- Nombre de valeurs à relever: NB_VALEURS + tableaux "etiquettes" à modifier selon abonnement (ici triphasé heures creuses).
 
Compilation PC:  
- gcc -Wall teleinfo.c -o teleinfo
 
Compilation wrt54gl: 
- avec le SDK (OpenWrt-SDK-Linux).
Fonctionne sur OpenWrt whiterussian/0.9 et kamikaze/8.09 (seul le Makefile et nom port série différent)
*/
 
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include <termios.h>
#include <sys/fcntl.h>
#include <sys/types.h>
 
// Define port serie
#define BAUDRATE B1200
#define SERIALPORT "/dev/ttyUSB0"
 
// Fichier local au Wrt4gl/PC + fichier trame pour debug.
#define TRAMELOG "/tmp/teleinfotrame."
 
// Active mode debug.
//#define DEBUG
 
//-----------------------------------------------------------------------------
 
// Déclaration pour le port série.
int             fdserial ;
struct termios  termiosteleinfo ;
 
// Déclaration pour les données.
char ch[2] ;
char car_prec ;
char message[512] ;
char* match;
char datateleinfo[512] ;
 
// Constantes/Variables à changer suivant abonnement.
#define NB_VALEURS 11			// Nombre de valeurs à relever, voir tableau "etiquettes", 20 pour abonnement tri heures creuse.
char etiquettes[NB_VALEURS][16] = {"ADCO", "OPTARIF", "ISOUSC", "HCHP", "HCHC", "PTEC", "IINST", "IMAX", "PAPP", "HHPHC", "MOTDETAT"} ;
// Fin Constantes/variables à changer suivant abonnement.
 
char 	valeurs[NB_VALEURS][18] ;
char	checksum[255] ;
int 	res ;
int	no_essais = 1 ;
int	nb_essais = 3 ;
int	erreur_checksum = 0 ;
 
// Déclaration pour la date.
time_t 		td;
struct 	tm 	*dc;
char		sdate[12];
char		sheure[10];
char		timestamp[11];
 
/*------------------------------------------------------------------------------*/
/* Init port rs232								*/
/*------------------------------------------------------------------------------*/
int initserie(void)
// Mode Non-Canonical Input Processing, Attend 1 caractère ou time-out(avec VMIN et VTIME).
{
	int device ;
 
        // Ouverture de la liaison serie (Nouvelle version de config.)
        if ( (device=open(SERIALPORT, O_RDWR | O_NOCTTY)) == -1 ) 
	{
                syslog(LOG_ERR, "Erreur ouverture du port serie %s !", SERIALPORT);
                exit(1) ;
        }
 
        tcgetattr(device,&termiosteleinfo) ;				// Lecture des parametres courants.
 
	cfsetispeed(&termiosteleinfo, BAUDRATE) ;			// Configure le débit en entrée/sortie.
	cfsetospeed(&termiosteleinfo, BAUDRATE) ;
 
	termiosteleinfo.c_cflag |= (CLOCAL | CREAD) ;			// Active réception et mode local.
 
	// Format série "7E1"
	termiosteleinfo.c_cflag |= PARENB  ;				// Active 7 bits de donnees avec parite pair.
	termiosteleinfo.c_cflag &= ~PARODD ;
	termiosteleinfo.c_cflag &= ~CSTOPB ;
	termiosteleinfo.c_cflag &= ~CSIZE ;
	termiosteleinfo.c_cflag |= CS7 ;
 
	termiosteleinfo.c_iflag |= (INPCK | ISTRIP) ;			// Mode de control de parité.
 
	termiosteleinfo.c_cflag &= ~CRTSCTS ;				// Désactive control de flux matériel.
 
	termiosteleinfo.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG) ;	// Mode non-canonique (mode raw) sans echo.
 
	termiosteleinfo.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL) ;	// Désactive control de flux logiciel, conversion 0xOD en 0x0A.
 
	termiosteleinfo.c_oflag &= ~OPOST ;				// Pas de mode de sortie particulier (mode raw).
 
	termiosteleinfo.c_cc[VTIME] = 80 ;  				// time-out à ~8s.
	termiosteleinfo.c_cc[VMIN]  = 0 ;   				// 1 car. attendu.
 
	tcflush(device, TCIFLUSH) ;					// Efface les données reçues mais non lues.
        tcsetattr(device,TCSANOW,&termiosteleinfo) ;			// Sauvegarde des nouveaux parametres
	return device ;
}
 
/*------------------------------------------------------------------------------*/
/* Lecture données téléinfo sur port série					*/
/*------------------------------------------------------------------------------*/
void LiTrameSerie(int device)
{
// (0d 03 02 0a => Code fin et début trame)
	tcflush(device, TCIFLUSH) ;			// Efface les données non lus en entrée.
	message[0]='\0' ;
	memset(valeurs, 0x00, sizeof(valeurs)) ; 
 
	do
	{
		car_prec = ch[0] ;
		res = read(device, ch, 1) ;
		if (! res)
		{	
			syslog(LOG_ERR, "Erreur pas de réception début données Téléinfo !\n") ;
			close(device);
			exit(1) ;
		}
	 }
	while ( ! (ch[0] == 0x02 && car_prec == 0x03) ) ;	// Attend code fin suivi de début trame téléinfo .
 
	do
	{
		res = read(device, ch, 1) ;
		if (! res)
		{	
			syslog(LOG_ERR, "Erreur pas de réception fin données Téléinfo !\n") ;
			close(device);
			exit(1) ;
		}
		ch[1] ='\0' ;
		strcat(message, ch) ;
	}
	while (ch[0] != 0x03) ;				// Attend code fin trame téléinfo.
}
 
/*------------------------------------------------------------------------------*/
/* Test checksum d'un message (Return 1 si checkum ok)				*/
/*------------------------------------------------------------------------------*/
int checksum_ok(char *etiquette, char *valeur, char checksum) 
{
	unsigned char sum = 32 ;		// Somme des codes ASCII du message + un espace
	int i ;
 
	for (i=0; i < strlen(etiquette); i++) sum = sum + etiquette[i] ;
	for (i=0; i < strlen(valeur); i++) sum = sum + valeur[i] ;
	sum = (sum & 63) + 32 ;
	if ( sum == checksum) return 1 ;	// Return 1 si checkum ok.
	#ifdef DEBUG
		syslog(LOG_INFO, "Checksum lu:%02x   calculé:%02x", checksum, sum) ;
	#endif
	return 0 ;
}
 
/*------------------------------------------------------------------------------*/
/* Recherche valeurs des étiquettes de la liste.				*/
/*------------------------------------------------------------------------------*/
int LitValEtiquettes()
{
	int id ;
	erreur_checksum = 0 ;
 
	for (id=0; id<NB_VALEURS; id++)
	{
		if ( (match = strstr(message, etiquettes[id])) != NULL)
		{
			sscanf(match, "%s %s %s", etiquettes[id], valeurs[id], checksum) ;
			if ( strlen(checksum) > 1 ) checksum[0]=' ' ;	// sscanf ne peux lire le checksum à 0x20 (espace), si longueur checksum > 1 donc c'est un espace.
			if ( ! checksum_ok(etiquettes[id], valeurs[id], checksum[0]) ) 
			{
				syslog(LOG_ERR, "Donnees teleinfo [%s] corrompues (essai %d) !\n", etiquettes[id], no_essais) ;
				erreur_checksum = 1 ;
				return 0 ;
			}
		}
	}
	// Remplace chaine "HP.." ou "HC.." par "HP ou "HC".
	valeurs[1][2] = '\0' ;
	#ifdef DEBUG
	printf("----------------------\n") ; for (id=0; id<NB_VALEURS; id++) printf("%s='%s'\n", etiquettes[id], valeurs[id]) ;
	#endif
	return 1 ;
}
 
#ifdef DEBUG
/*------------------------------------------------------------------------------*/
/* Ecrit la trame teleinfo dans fichier si erreur (pour debugger)		*/
/*------------------------------------------------------------------------------*/
void writetrameteleinfo(char trame[], char ts[])
{
	char nomfichier[255] = TRAMELOG ;
	strcat(nomfichier, ts) ;
        FILE *teleinfotrame ;
        if ((teleinfotrame = fopen(nomfichier, "w")) == NULL)
        {
		syslog(LOG_ERR, "Erreur ouverture fichier teleinfotrame %s !", nomfichier) ;
                exit(1);
        }
        fprintf(teleinfotrame, "%s", trame) ;
        fclose(teleinfotrame) ;
}
#endif
 
/*------------------------------------------------------------------------------*/
/* Main										*/
/*------------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
 
 openlog("teleinfoserial", LOG_PID, LOG_USER) ;
 fdserial = initserie() ;
 
 do
 {
	// Lit trame téléinfo.
	LiTrameSerie(fdserial) ;
 
	time(&td) ;                                     // Lit date/heure système.
	dc = localtime(&td) ;
	strftime(sdate,sizeof sdate,"%Y-%m-%d",dc);
	strftime(sheure,sizeof sdate,"%H:%M:%S",dc);
	strftime(timestamp,sizeof timestamp,"%s",dc);
 
	#ifdef DEBUG
	writetrameteleinfo(message, timestamp) ;	// Enregistre trame en mode debug.
	#endif
 
	if ( LitValEtiquettes() ) 			// Lit valeurs des étiquettes de la liste.
	{
		long hchc = atol(valeurs[4]);
		long hchp = atol(valeurs[3]);
		long iinst = atol(valeurs[6]);
		long papp = atol(valeurs[8]);
		
		printf("PUTVAL \"pi/exec-teleinfo/gauge-hchc\" interval=60 N:%lu\n", hchc) ;
		printf("PUTVAL \"pi/exec-teleinfo/gauge-hchp\" interval=60 N:%lu\n", hchp) ;
		printf("PUTVAL \"pi/exec-teleinfo/gauge-iinst\" interval=60 N:%lu\n", iinst) ;
		printf("PUTVAL \"pi/exec-teleinfo/gauge-papp\" interval=60 N:%lu\n", papp) ;
	}
	#ifdef DEBUG
	else writetrameteleinfo(message, timestamp) ;	// Si erreur checksum enregistre trame.
	#endif
	no_essais++ ;
 }
 while ( (erreur_checksum) && (no_essais <= nb_essais) ) ;
 
 close(fdserial) ;
 closelog() ;
 exit(0) ;
}
