#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "protocole.h" // contient la cle et la structure d'un message
#include "FichierClient.h" // module de gestion des clients

int idQ,idShm,idSem;
int fdPipe[2];
TAB_CONNEXIONS *tab;

void afficheTab();

void handlerSIGINT(int sig);

void handlerSIGCHLD(int sig);

int rechercheTableConnexion(int pid);

int rechercheCaddieTableConnexion(int pid);

int main()
{
  int index_tab;
  // Armement des signaux
  // TO DO

  struct sigaction B;
  B.sa_handler = handlerSIGINT;
  B.sa_flags = 0;
  sigemptyset(&B.sa_mask);
  sigaction(SIGINT, &B, NULL);

  struct sigaction C;
  C.sa_handler = handlerSIGCHLD;
  C.sa_flags = 0;
  sigemptyset(&C.sa_mask);
  sigaction(SIGCHLD, &C, NULL);

  // Creation des ressources
  // Creation de la file de message
  fprintf(stderr,"(SERVEUR %d) Creation de la file de messages\n",getpid());
  if ((idQ = msgget(CLE,IPC_CREAT | IPC_EXCL | 0600)) == -1)  // CLE definie dans protocole.h
  {
    perror("(SERVEUR) Erreur de msgget");
    exit(1);
  }

  // TO BE CONTINUED

  // Creation du pipe
  // TO DO

  // Initialisation du tableau de connexions
  tab = (TAB_CONNEXIONS*) malloc(sizeof(TAB_CONNEXIONS)); 

  for (int i=0 ; i<6 ; i++)
  {
    tab->connexions[i].pidFenetre = 0;
    strcpy(tab->connexions[i].nom,"");
    tab->connexions[i].pidCaddie = 0;
  }
  tab->pidServeur = getpid();
  tab->pidPublicite = 0;

  afficheTab();

  // Creation du processus Publicite (étape 2)
  // TO DO

  // Creation du processus AccesBD (étape 4)
  // TO DO

  MESSAGE m;
  MESSAGE reponse;

  while(1)
  {
  	fprintf(stderr,"(SERVEUR %d) Attente d'une requete...\n",getpid());
    if (msgrcv(idQ,&m,sizeof(MESSAGE)-sizeof(long),1,0) == -1)
    {
      perror("(SERVEUR) Erreur de msgrcv");
      msgctl(idQ,IPC_RMID,NULL);
      exit(1);
    }

    switch(m.requete)
    {
      case CONNECT :
                      fprintf(stderr,"(SERVEUR %d) Requete CONNECT reçue de %d\n",getpid(),m.expediteur);
                      for(int i = 0; i<6; i++){
                        if(tab->connexions[i].pidFenetre == 0){
                          tab->connexions[i].pidFenetre = m.expediteur;
                          break;
                        }
                      }
                      break;

      case DECONNECT :
                      fprintf(stderr,"(SERVEUR %d) Requete DECONNECT reçue de %d\n",getpid(),m.expediteur);
                      if((index_tab = rechercheTableConnexion(m.expediteur)) != -1)
                        tab->connexions[index_tab].pidFenetre = 0;
                      break;
      case LOGIN :    // TO DO
                      fprintf(stderr,"(SERVEUR %d) Requete LOGIN reçue de %d : --%d--%s--%s--\n",getpid(),m.expediteur,m.data1,m.data2,m.data3);
                      reponse.data1 = 1;
                      if(m.data1 == 1){//Nouveau Client
                        if(estPresent(m.data2)){
                          reponse.data1 = 0;
                          strcpy(reponse.data4, "Nouveau Client impossible : Client existe déjà");
                        }
                        else{
                          ajouteClient(m.data2, m.data3);
                        }
                      }
                      else{//Connexion Client déjà existant
                        int pos = estPresent(m.data2);
                        if(!pos){
                          reponse.data1 = 0;
                          strcpy(reponse.data4, "Tentative de connexion à un client inexistant");
                        }
                        else if(!verifieMotDePasse(pos, m.data3)){
                          reponse.data1 = 0;
                          strcpy(reponse.data4, "Mot de passe incorrect");
                        }
                      }




                      if(reponse.data1){//LOGIN success
                        index_tab = rechercheTableConnexion(m.expediteur);
                        if(index_tab != -1)//Vérifie que le client qui veut se LOGIN est connecté au serveur
                          strcpy(tab->connexions[index_tab].nom, m.data2);

                        pid_t pid_caddie = fork();
                        //Si erreur de fork, annule le LOGIN
                        if(pid_caddie == -1){//Erreur fork
                          perror("Erreur de fork");
                          reponse.data1 = 0;
                          strcpy(reponse.data4, "Impossible de créer le Caddie");
                          strcpy(tab->connexions[index_tab].nom, "");
                        }
                        else if(pid_caddie == 0){//Caddie
                          if (execl("./Caddie", "Caddie", NULL) == -1){
                            perror("Erreur de execl()");
                            exit(1);
                          }
                        }
                        else{//Serveur -> stock pid Caddie
                          tab->connexions[index_tab].pidCaddie = pid_caddie;
                        }

                      }

                      //Envoi du feedback de login au client
                      reponse.type = m.expediteur;
                      reponse.expediteur = getpid();
                      reponse.requete = LOGIN;

                      if (msgsnd(idQ, &reponse, sizeof(MESSAGE) - sizeof(long), 0) == -1){
                        perror("Erreur de msgsnd");
                        exit(1);
                      }
                      kill(m.expediteur, SIGUSR1);
                      break;

      case LOGOUT :  // TO DO
                      fprintf(stderr,"(SERVEUR %d) Requete LOGOUT reçue de %d\n",getpid(), m.expediteur);
                      
                      if((index_tab = rechercheTableConnexion(m.expediteur))!= -1){
                        strcpy(tab->connexions[index_tab].nom, "");
                      }
                      break;

      case UPDATE_PUB : // TO DO
                      break;

      case CONSULT :  
                      fprintf(stderr,"(SERVEUR %d) Requete CONSULT reçue de %d\n",getpid(),m.expediteur);
                      
                      index_tab = rechercheTableConnexion(m.expediteur);
                  
                      if(index_tab != -1){
                        m.type = tab->connexions[index_tab].pidCaddie;
                        m.expediteur = getpid();

                        if (msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long), 0) == -1){
                          perror("Erreur de msgsnd");
                          exit(1);
                        }
                      }
                      break;

      case ACHAT :    // TO DO
                      fprintf(stderr,"(SERVEUR %d) Requete ACHAT reçue de %d\n",getpid(),m.expediteur);
                      break;

      case CADDIE :  // TO DO
                      fprintf(stderr,"(SERVEUR %d) Requete CADDIE reçue de %d\n",getpid(),m.expediteur);
                      break;

      case CANCEL :  // TO DO
                      fprintf(stderr,"(SERVEUR %d) Requete CANCEL reçue de %d\n",getpid(),m.expediteur);
                      break;

      case CANCEL_ALL : // TO DO
                      fprintf(stderr,"(SERVEUR %d) Requete CANCEL_ALL reçue de %d\n",getpid(),m.expediteur);
                      break;

      case PAYER : // TO DO
                      fprintf(stderr,"(SERVEUR %d) Requete PAYER reçue de %d\n",getpid(),m.expediteur);
                      break;

      case NEW_PUB :  // TO DO
                      fprintf(stderr,"(SERVEUR %d) Requete NEW_PUB reçue de %d\n",getpid(),m.expediteur);
                      break;
    }
    afficheTab();
  }
}

void afficheTab()
{
  fprintf(stderr,"Pid Serveur  : %d\n",tab->pidServeur);
  fprintf(stderr,"Pid Publicite : %d\n",tab->pidPublicite);
  fprintf(stderr,"Pid AccesBD  : %d\n",tab->pidAccesBD);
  for (int i=0 ; i<6 ; i++)
    fprintf(stderr,"%6d -%20s- %6d\n",tab->connexions[i].pidFenetre,
                                                      tab->connexions[i].nom,
                                                      tab->connexions[i].pidCaddie);
  fprintf(stderr,"\n");
}

////////////////////////////////////////////////////////////////////////////////////

void handlerSIGINT(int sig){
  if (msgctl(idQ, IPC_RMID, NULL) == -1){
    perror("Erreur de msgctl");
    exit(1);
  }

  exit(0);
}

void handlerSIGCHLD(int sig){
  int id = wait(NULL);
  fprintf(stderr, "(SERVEUR %d) Suppression du fils %d de la table des processus\n", getpid(), id);

  int index_tab = rechercheCaddieTableConnexion(id);
  if(index_tab != -1)
    tab->connexions[index_tab].pidCaddie = 0;
}

int rechercheTableConnexion(int pid){
  for(int i=0; i<6; i++){
    if(tab->connexions[i].pidFenetre == pid)
      return i;
  }

  return -1;
}

int rechercheCaddieTableConnexion(int pid){
  for (int i = 0; i < 6; i++){
    if (tab->connexions[i].pidCaddie == pid)
      return i;
  }

  return -1;
}
